/*
     Compile with:
     g++ -std=c++11 -o space2super main.cpp -W -Wall -L/usr/X11R6/lib -lX11 -lXtst

     To install libx11:
     in Ubuntu: sudo apt-get install libx11-dev

     To install libXTst:
     in Ubuntu: sudo apt-get install libxtst-dev

     Needs module XRecord installed. To install it, add the line `Load "record"' to `Section "Module"'
     in /etc/X11/xorg.conf like this:

     Section "Module"
             Load    "record"
     EndSection

*/

#include <iostream>
#include <memory>

#include <sys/time.h>
#include <signal.h>

#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>


#ifndef NDEBUG
    #define LOG(x) std::cout << x << std::endl;
#else
    #define LOG(x)
#endif


static const int REMAPPED_KEYCODE = 255;
static const int TIMEOUT_MS = 600;

struct CallbackClosure {
    Display *ctrlDisplay;
    Display *dataDisplay;
    int curX;
    int curY;
    void *initialObject;
};


typedef union {
    unsigned char type;
    xEvent event;
    xResourceReq req;
    xGenericReply reply;
    xError error;
    xConnSetupPrefix setup;
} XRecordDatum;


class Space2Super {
private:
    std::string m_displayName;
    CallbackClosure userData;
    std::pair<int, int> recVer;
    XRecordRange *recRange;
    XRecordClientSpec recClientSpec;
    XRecordContext recContext;

private:
    void setupXTestExtension(){
        int _1, _2, _3, _4;
        if (! XTestQueryExtension(userData.ctrlDisplay, &_1, &_2, &_3, &_4)) {
            throw std::runtime_error("There is no XTest extension loaded by the X server.");
        }
    }

    void setupRecordExtension() {
        XSynchronize(userData.ctrlDisplay, True);

        // Does the record extension exist?
        if (! XRecordQueryVersion(userData.ctrlDisplay, &recVer.first, &recVer.second)) {
            throw std::runtime_error("There is no RECORD extension loaded by the X server.\n"
                "You must add the following line:\n"
                "     Load    \"record\"\n"
                "to /etc/X11/xorg.conf, into the `Module' section.");
        }

        recRange = XRecordAllocRange();
        if (recRange == nullptr) {
            throw std::runtime_error("Could not allocate record range object");
        }
        recRange->device_events.first = KeyPress;
        recRange->device_events.last = ButtonRelease;
        recClientSpec = XRecordAllClients;

        // Get context with our configuration
        recContext = XRecordCreateContext(userData.ctrlDisplay, 0, &recClientSpec, 1, &recRange, 1);
        if (recContext == 0) {
            throw std::runtime_error("Could not create a record context" );
        }
    }

    static int diffInMillisec(timeval t1, timeval t2) {
        return ((t1.tv_sec - t2.tv_sec) * 1000000 + (t1.tv_usec - t2.tv_usec)) / 1000;
    }

    // Called from Xserver when new event occurs.
    static void eventCallback(XPointer priv, XRecordInterceptData *hook) {
        if (hook->category != XRecordFromServer) {
            XRecordFreeData(hook);
            return;
        }

        CallbackClosure *userData = (CallbackClosure *) priv;
        XRecordDatum *data = (XRecordDatum *) hook->data;
        static bool space_down = false;
        static bool key_combo = false;
        static bool modifier_down = false;
        static struct timeval startWait, endWait;

        unsigned char t = data->event.u.u.type;
        int c = data->event.u.u.detail;

        LOG("\nState:");
        if (space_down) {
            LOG("space_down = true");
        } else {
            LOG("space_down = false");
        }

        if (key_combo) {
            LOG("key_combo = true");
        } else {
            LOG("key_combo = false");
        }

        if (modifier_down) {
            LOG("modifier_down = true");
        } else {
            LOG("modifier_down = false");
        }

        static const int SPACE = 65;
        static const int ALT = 108;

        switch (t) {
            case KeyPress: {
                LOG("KeyPress");
                if (c == SPACE) {
                    space_down = true;
                    gettimeofday(&startWait, nullptr);
                    LOG("        SPACE");
                } else if (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Super_L) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Super_R))
                {
                    LOG("        Super_{L||R}");
                    // Super_pressed
                    if (space_down) { // space-Super sequence
                        XTestFakeKeyEvent(userData->ctrlDisplay, REMAPPED_KEYCODE, True, CurrentTime);
                        XTestFakeKeyEvent(userData->ctrlDisplay, REMAPPED_KEYCODE, False, CurrentTime);
                    }
                } else if (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_L) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_R) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R) ||
                           c == ALT)
                {
                    LOG("        Modifier");
                    // TODO: Find a better way to get those modifiers!!!
                    modifier_down = true;
                } else {
                    LOG("        Another");
                    if (space_down) {
                        key_combo = true;
                    } else {
                        key_combo = false;
                    }
                }

                break;
            }
            case KeyRelease: {
                LOG("KeyRelease");
                if (c == SPACE) {
                    LOG("        SPACE");
                    space_down = false; // space released

                    if (! key_combo && ! modifier_down) {
                        LOG("        (! key_combo && ! modifier_down)");
                        gettimeofday(&endWait, nullptr);
                        if (diffInMillisec(endWait, startWait) < TIMEOUT_MS) {
                            // if a minimum timeout has elapsed since space was pressed
                            XTestFakeKeyEvent(userData->ctrlDisplay, REMAPPED_KEYCODE, True, CurrentTime);
                            XTestFakeKeyEvent(userData->ctrlDisplay, REMAPPED_KEYCODE, False, CurrentTime);
                        }
                    }
                    key_combo = false;
                } else if (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Super_L) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Super_R))
                {
                    LOG("        Super_{L||R}");
                    // Super release
                    if (space_down) {
                        key_combo = true;
                    }
                } else if (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_L) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_R) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R) ||
                           c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R) ||
                           c == ALT)
                {
                    LOG("        Modifier");
                    // TODO: Find a better way to get those modifiers!!!
                    modifier_down = false;
                }

                break;
            }
            case ButtonPress: {
                if (space_down) {
                    key_combo = true;
                } else {
                    key_combo = false;
                }

                break;
            }
        }

        XRecordFreeData(hook);
    }

public:
    Space2Super() {
    }

    ~Space2Super() {
        stop();
    }

    bool connect(const std::string& displayName) {
        m_displayName = displayName;
        if ((userData.ctrlDisplay = XOpenDisplay(m_displayName.c_str())) == nullptr) {
            return false;
        }
        if ((userData.dataDisplay = XOpenDisplay(m_displayName.c_str())) == nullptr) {
            XCloseDisplay(userData.ctrlDisplay);
            userData.ctrlDisplay = nullptr;
            return false;
        }

        // You may want to set a custom X error handler here

        userData.initialObject = (void *) this;
        setupXTestExtension();
        setupRecordExtension();

        return true;
    }

    void start() {
        // Remap keycode REMAPPED_KEYCODE to Keysym space:
        /*
        KeySym spc = XK_space;
        XChangeKeyboardMapping(userData.ctrlDisplay, REMAPPED_KEYCODE, 1, &spc, 1);
        XFlush(userData.ctrlDisplay);
        */

        /*
        XTestFakeKeyEvent(userData.ctrlDisplay, REMAPPED_KEYCODE, True, CurrentTime);
        XTestFakeKeyEvent(userData.ctrlDisplay, REMAPPED_KEYCODE, False, CurrentTime);
        */

        if (! XRecordEnableContext(userData.dataDisplay, recContext, eventCallback, (XPointer) &userData)) {
            throw std::runtime_error("Couldn't enable a record context");
        }
    }

    void stop() {
        if (! XRecordDisableContext (userData.ctrlDisplay, recContext)) {
            throw std::runtime_error("Couldn't disable a record context");
        }
    }
};

void stop(int param) {
    if (param == SIGTERM) {
        std::clog << "-- Caught a SIGTERM --" << std::endl;
    }
    exit(1);
}

int main() {
    signal(SIGTERM, stop);

    Space2Super space2super;

    std::clog << "-- Starting Space2Super --" << std::endl;
    if (space2super.connect(":0")) {
        space2super.start();
    }
}
