/*
    Compile with:
        g++ -std=c++11 -o space2super space2super.cpp -W -Wall -L/usr/X11R6/lib -lX11 -lXtst
    or equivalently:
        make

    To install libXTst in Ubuntu:
        sudo apt-get install libxtst-dev
    or equivalently:
        make deps

    Needs the XRecord extension installed.
    Try adding the following line into the `Module` section of /etc/X11/xorg.conf:
        Load    "record"

    X Record API documentation is available at:
        https://www.xfree86.org/current/recordlib.pdf
    X Keyboard Extension (XKB) API documentation is available at:
        https://www.xfree86.org/current/XKBproto.pdf
*/

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include <sys/time.h>
#include <signal.h>

#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>

// Defined in Xlibint.h.
#undef min
#undef max


#ifndef NDEBUG
    #define LOG(x) std::clog << x << std::endl;
#else
    #define LOG(x)
#endif


const char* DRIVER = "s2sctl";


class Space2Super {
public:
    struct InitializationError: public std::exception {};

public:
    Space2Super(KeyCode original_space_key_code, int timeout_millisec):
        original_space_key_code_(original_space_key_code),
        timeout_millisec_(timeout_millisec)
    {
        if (! initialize()) {
            throw InitializationError();
        }
    }

    void run() {
        if (! start_loop()) {
            throw InitializationError();
        }
    }

    ~Space2Super() {
        stop();
    }

private:
    class DisplayCloser {
    public:
        void operator()(Display* display) {
            XCloseDisplay(display);
        }
    };

    class XObjectDestructor {
    public:
        void operator()(void* object) {
            XFree(object);
        }
    };

    class XRecordInterceptDataDestructor {
    public:
        void operator()(XRecordInterceptData* intercept_data) {
            XRecordFreeData(intercept_data);
        }
    };

    class KeyCodeSet {
    public:
        bool contains(const KeyCode& key_code) const {
            return contains_[key_code];
        }

        void map(const std::function<void(KeyCode)>& process) const {
            for_each_key_code([this, &process](KeyCode key_code) {
                if (contains(key_code)) {
                    process(key_code);
                }
            });
        }

        void populate_key_codes(const std::function<bool(KeyCode)>& is_contained) {
            for_each_key_code([this, &is_contained](KeyCode key_code) {
                contains_[key_code] = is_contained(key_code);
            });
        }

        // Uses the no-modifier version of the key code mappings (1st column of `xmodmap -pke` output).
        void populate_key_syms(Display* display, const std::function<bool(KeySym)>& is_contained) {
            populate_key_codes([&display, &is_contained](KeyCode key_code) -> bool {
                return is_contained(XkbKeycodeToKeysym(display, key_code, /* group */ 0, /* shift */ 0));
            });
        }

    private:
        static void for_each_key_code(const std::function<void(KeyCode)>& process) {
            // Can't do this in a simple `for` loop because e.g. `key_code <= MAX_KEY_CODE_` is a tautology.
            KeyCode key_code = MIN_KEY_CODE_;
            do {
                process(key_code);
            } while (++key_code != MIN_KEY_CODE_);  // Wrapped around `MAX_KEY_CODE_`.
        }

    private:
        typedef std::numeric_limits<KeyCode> KeyCodeLimits;

    private:
        static constexpr KeyCode MIN_KEY_CODE_ = KeyCodeLimits::min();
        static constexpr KeyCode MAX_KEY_CODE_ = KeyCodeLimits::max();
        static_assert(MIN_KEY_CODE_ == 0 && MAX_KEY_CODE_ == 255, "Expected `KeyCode` to be an unsigned byte");

        static constexpr int KEY_CODE_COUNT_ = MAX_KEY_CODE_ + 1;

    private:
        bool contains_[KEY_CODE_COUNT_] = {};
    };

public:
    friend std::ostream& operator <<(std::ostream&, const KeyCodeSet&);

private:
    typedef std::unique_ptr<Display, DisplayCloser> DisplayPointer;

private:
    // The key code that was originally mapped to the Space key (used to detect Space key presses).
    KeyCode original_space_key_code_;

    // The maximum amount of milliseconds during which Space can be pressed to be typed.
    int timeout_millisec_;

    // The synthetic key code that will fire when Space is to be typed, see `s2sctl`.
    KeyCode remapped_key_code_;

    // See Section 1.3 of the XRecord API specification which recommends to open two connections
    // and directs which connection is typically used with each XRecord API call
    // (presumably because of the blocking nature of `XRecordEnableContext`.
    // A control connection to the X Server ("for recording control").
    DisplayPointer control_display_;
    // A data connection to the X Server ("for reading recorded protocol data").
    DisplayPointer data_display_;

    XRecordContext record_context_;

    KeyCodeSet super_keys_;
    KeyCodeSet modifier_keys_;

    // Whether Space is pressed.
    bool space_down_ = false;
    // If yes, indicates when the `KeyPress` event happened.
    timeval space_down_moment_;

    // Whether any of Control/Shift/Alt is pressed.
    bool modifier_down_ = false;

    // Whether Space is pressed simultaneously with some other keys (so should not be typed).
    bool space_key_combo_ = false;

private:
    bool check_xtest_extension() const {
        int unused;
        if (! XTestQueryExtension(control_display_.get(), &unused, &unused, &unused, &unused)) {
            std::cerr << "The XTest extension has not been loaded by the X server." << std::endl;
            return false;
        }
        return true;
    }

    bool check_xrecord_extension() const {
        int unused;
        if (! XRecordQueryVersion(control_display_.get(), &unused, &unused)) {
            std::cerr <<
                "The XRecord extension has not been loaded by the X server.\n" <<
                "Try adding the following line:\n"
                "     Load    \"record\"\n"
                "into the `Module` section of /etc/X11/xorg.conf." <<
                std::endl;
            return false;
        }
        return true;
    }

    bool setup_key_codes() {
        remapped_key_code_ = XKeysymToKeycode(control_display_.get(), XK_space);
        if (remapped_key_code_ == 0) {
            std::cerr
                << "Couldn't map the `XK_space` KeySym back to a key code."
                << "You may need to run `xmodmap -e 'keycode any = space'`"
                << "(normally `s2sctl` takes care of this)."
                << std::endl;
            return false;
        }

        LOG("Key code mapping:");

        LOG("  Space (original): " << static_cast<int>(original_space_key_code_));
        LOG("  Space (remapped): " << static_cast<int>(remapped_key_code_));

        super_keys_.populate_key_syms(control_display_.get(), [](KeySym key_sym) -> bool {
            return key_sym == XK_Super_L || key_sym == XK_Super_R;
        });
        LOG("  Super_{L|R}: " << super_keys_);

        modifier_keys_.populate_key_syms(control_display_.get(), [](KeySym key_sym) -> bool {
            // Do not include XKB latches/locks as those are not parts of key combinations.
            // Do not include Super as it is treated separately in `super_keys_`.
            switch (key_sym) {
            case XK_Shift_L:
            case XK_Shift_R:
            case XK_Control_L:
            case XK_Control_R:
            case XK_Meta_L:
            case XK_Meta_R:
            case XK_Alt_L:
            case XK_Alt_R:
            case XK_Hyper_L:
            case XK_Hyper_R:
#ifdef XK_XKB_KEYS
            case XK_ISO_Lock:
            case XK_ISO_Level3_Shift:
            case XK_ISO_Level5_Shift:
            case XK_ISO_Group_Shift:
            case XK_ISO_Next_Group:
            case XK_ISO_Prev_Group:
            case XK_ISO_First_Group:
            case XK_ISO_Last_Group:
#else
            // `XK_ISO_Group_Shift` is an alias for `XK_Mode_switch`.
            case XK_Mode_switch:
#endif
                return true;
            default:
                return false;
            }
        });
        LOG("  Modifiers: " << modifier_keys_);

        return true;
    }

    bool open_display(DisplayPointer& display) {
        display.reset(XOpenDisplay(/* display_name */ nullptr));  // $DISPLAY by default.
        if (display == nullptr) {
            std::cerr << "Could not open the default display (not running under X11?)." << std::endl;
            return false;
        }
        return true;
    }

    bool initialize() {
        LOG("Initializing Space2Super...");

        if (! open_display(control_display_) ||
            ! open_display(data_display_) ||
            ! check_xtest_extension() ||
            ! check_xrecord_extension())
        {
            return false;
        }

        // Requires Xlib to report errors as they occur.
        XSynchronize(control_display_.get(), True);

        if (! setup_key_codes()) {
            return false;
        }

        LOG("Space2Super initialized successfully.");
        return true;
    }

    bool start_loop() {
        LOG("Starting Space2Super event loop...");

        XRecordClientSpec record_client_spec = XRecordAllClients;
        XRecordClientSpec record_client_specs[] = {record_client_spec};

        std::unique_ptr<XRecordRange, XObjectDestructor> record_range{XRecordAllocRange()};
        if (record_range == nullptr) {
            std::cerr << "Could not allocate a record range object (XRecordRange)." << std::endl;
            return false;
        }
        record_range->device_events.first = KeyPress;
        record_range->device_events.last = ButtonRelease;
        XRecordRange* record_ranges[] = {record_range.get()};

        record_context_ = XRecordCreateContext(
            control_display_.get(), /* datum_flags */ 0 /* disable all options */,
            record_client_specs, /* nclients */ std::extent<decltype(record_client_specs)>::value,
            record_ranges, /* n_ranges */ std::extent<decltype(record_ranges)>::value
        );

        if (record_context_ == 0) {
            std::cerr << "Could not create a record context (XRecordContext)." << std::endl;
            return false;
        }

        // The application will wait for this synchronous call until `stop` is invoked.
        auto status = XRecordEnableContext(
            data_display_.get(), record_context_, event_callback, reinterpret_cast<XPointer>(this)
        );
        if (status == 0) {
            std::cerr << "Couldn't enable the record context." << std::endl;
            return false;
        }

        LOG("Space2Super event loop complete.");
        return true;
    }

    static int timespan_milliseconds(const timeval& start, const timeval& end) {
        return
            (end.tv_sec - start.tv_sec) * 1000 +
            (end.tv_usec - start.tv_usec) / 1000;
    }

    static const char* yes_or_no(bool value) {
        return value ? "yes" : "no";
    };

    bool space_down_alone() const {
        return space_down_ && ! (space_key_combo_ || modifier_down_);
    }

    void log_state(const char* description) const {
        (void)description;  // Prevent flagging as unused on NDEBUG.
        LOG(description << ":" <<
            "  Space down: " << yes_or_no(space_down_) <<
            "  Key combination: " << yes_or_no(space_key_combo_) <<
            "  Modifier(s) down: " << yes_or_no(modifier_down_) <<
            "  Space alone: " << yes_or_no(space_down_alone())
        );
    }

    bool is_space(KeyCode key_code) const {
        if (key_code == original_space_key_code_) {
            LOG("  Space");
            return true;
        }
        return false;
    }

    bool is_super(KeyCode key_code) const {
        if (super_keys_.contains(key_code)) {
            LOG("  Super_{L|R}");
            return true;
        }
        return false;
    }

    bool is_modifier(KeyCode key_code) const {
        if (modifier_keys_.contains(key_code)) {
            LOG("  Modifier: {Control|Shift|Alt}_{L|R}");
            return true;
        }
        return false;
    }

    void handle_key_press(KeyCode key_code) {
        LOG("KeyPress");

        if (is_space(key_code)) {
            space_down_ = true;
            gettimeofday(&space_down_moment_, nullptr);
        } else if (is_super(key_code)) {
            if (space_down_) {  // Space-Super sequence.
                XTestFakeKeyEvent(control_display_.get(), remapped_key_code_, True, CurrentTime);
                XTestFakeKeyEvent(control_display_.get(), remapped_key_code_, False, CurrentTime);
            }
        } else if (is_modifier(key_code)) {
            modifier_down_ = true;
        } else {
            LOG("  Other: "
                << XKeysymToString(XkbKeycodeToKeysym(control_display_.get(), key_code, /* group */ 0, /* shift */ 0))
                << ", key code " << static_cast<int>(key_code)
            );
            space_key_combo_ = space_down_;
        }
    }

    void handle_key_release(KeyCode key_code) {
        LOG("KeyRelease");

        if (is_space(key_code)) {
            if (space_down_alone()) {
                struct timeval space_release_moment;
                gettimeofday(&space_release_moment, nullptr);
                int space_held_milliseconds = timespan_milliseconds(space_down_moment_, space_release_moment);
                LOG("  Released alone; " << space_held_milliseconds <<
                    " ms passed since it was pressed, the limit is " << timeout_millisec_ << " ms");

                // If a minimum timeout has elapsed since space was pressed...
                if (space_held_milliseconds <= timeout_millisec_) {
                    LOG("  Simulating key press, code " << static_cast<int>(remapped_key_code_));
                    XTestFakeKeyEvent(control_display_.get(), remapped_key_code_, True, CurrentTime);
                    XTestFakeKeyEvent(control_display_.get(), remapped_key_code_, False, CurrentTime);
                }
            }

            space_down_ = false;
            space_key_combo_ = false;
        } else if (is_super(key_code)) {
            if (space_down_) {
                space_key_combo_ = true;
            }
        } else if (is_modifier(key_code)) {
            modifier_down_ = false;
        }
    }

    void handle_button_press() {
        LOG("ButtonPress");
        space_key_combo_ = space_down_;
    }

    void process_event(KeyCode event_type, KeyCode key_code) {
        switch (event_type) {
        case KeyPress:
        case KeyRelease:
        case ButtonPress:
            LOG("");  // Separate event reports with blank lines.
            log_state("State before");
            break;
        default:
            return;
        }

        switch (event_type) {
        case KeyPress:
            handle_key_press(key_code);
            break;
        case KeyRelease:
            handle_key_release(key_code);
            break;
        case ButtonPress:
            handle_button_press();
            break;
        }

        log_state("State after");
    }

    // Called from the X server when a new event occurs.
    static void event_callback(
        XPointer callback_closure, XRecordInterceptData* intercept_data)
    {
        std::unique_ptr<XRecordInterceptData, XRecordInterceptDataDestructor> data{intercept_data};

        if (data->category != XRecordFromServer) {
            return;
        }

        const xEvent& event = *reinterpret_cast<xEvent*>(intercept_data->data);
        const auto& generic_event = event.u.u;
        KeyCode event_type = generic_event.type;
        KeyCode key_code = generic_event.detail;

        auto self = reinterpret_cast<Space2Super*>(callback_closure);
        self->process_event(event_type, key_code);
    }

    void stop() {
        LOG("Stopping Space2Super event loop...");
        if (! XRecordDisableContext (control_display_.get(), record_context_)) {
            std::cerr << "Couldn't disable the record context." << std::endl;
        }
        XRecordFreeContext(control_display_.get(), record_context_);
    }
};

std::ostream& operator <<(std::ostream& stream, const Space2Super::KeyCodeSet& key_codes) {
    const char* separator = "";
    key_codes.map([&stream, &key_codes, &separator](KeyCode key_code) {
        if (key_codes.contains(key_code)) {
            stream << separator << static_cast<int>(key_code);
            separator = " ";
        }
    });
    return stream;
}

std::unique_ptr<Space2Super> instance;

void stop(int signal_number) {
    LOG("Received signal " << signal_number << ".");
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        LOG("Destroying Space2Super.");
        instance.reset();
        LOG("Exiting.");
        exit(EXIT_SUCCESS);
    } else {
        throw std::logic_error("Should not receive signals other than SIGINT and SIGTERM");
    }
}

int main(const int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Use `" << DRIVER << "` to start/stop Space2Super" << std::endl;
        return EXIT_FAILURE;
    }

    KeyCode original_space_key_code = static_cast<KeyCode>(atoi(argv[1]));
    int timeout = atoi(argv[2]);

    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    try {
        Space2Super space2super(original_space_key_code, timeout);
        // Will loop until the destructor is called from `stop`.
        space2super.run();
    } catch (const Space2Super::InitializationError&) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
