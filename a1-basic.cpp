#include <cstdlib>
#include <iostream>
#include <queue>
#include <list>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "simon.h"
#include <unistd.h>
#include <sys/time.h>
#include <cmath>

using namespace std;

/*
 * Information to draw on the window.
 */
struct XInfo
{
    int width;
    int height;
    int screen;
    Display *display;
    Window window;
    GC gc;
    GC gc1;
};

XInfo xinfo;
queue<int> clicked;

/*
 * An abstract class representing displayable things.
 */
class Displayable
{
  public:
    virtual void paint(XInfo &xinfo) = 0;
    virtual void setXY(int _x, int _y) {}
    virtual int getX() = 0;
    virtual int getY() = 0;
    virtual bool isCircle() = 0;
};

/*
 * A text displayable
 */
class Text : public Displayable
{
  public:
    virtual void paint(XInfo &xinfo)
    {
        XDrawImageString(xinfo.display, xinfo.window, xinfo.gc,
                         this->x, this->y, this->s.c_str(), this->s.length());
    }

    void changeText(string text)
    {
        s = text;
    }

    virtual int getX()
    {
        return x;
    }
    virtual int getY()
    {
        return y;
    }

    bool isCircle()
    {
        return false;
    }

    // constructor
    Text(int x, int y, string s) : x(x), y(y), s(s) {}

  private:
    int x;
    int y;
    string s; // string to show
};

/*
 * A circle displayable
 */
class Circle : public Displayable
{
  public:
    virtual void paint(XInfo &xinfo)
    {
        // draw either thick or thin line depending on if the mouse is on it
        if (currentHover == 1)
        {
            XSetLineAttributes(xinfo.display, xinfo.gc1,
                               4, // 4 is line width
                               LineSolid, CapButt, JoinRound);
            XDrawArc(xinfo.display, xinfo.window, xinfo.gc1, x, y, d, d, 0, 360 * 64);
        }
        else
        {
            // draw circle
            XDrawArc(xinfo.display, xinfo.window, xinfo.gc, x, y, d, d, 0, 360 * 64);
        }
        XDrawImageString(xinfo.display, xinfo.window, xinfo.gc,
                         x + 50, y + 50, to_string(val).c_str(), to_string(val).length());
    }

    virtual void setXY(int _x, int _y)
    {
        x = _x;
        y = _y;
    }

    virtual int getX()
    {
        return x;
    }
    virtual int getY()
    {
        return y;
    }

    virtual bool isCircle()
    {
        return true;
    }

    void mouseHover(int mx, int my, int n)
    {
        float dist = sqrt(pow(mx - x - 50, 2) + pow(my - y - 50, 2));
        int space = (xinfo.width - 100 * n) / (2 + n - 1);
        int i = (x - space) / (space + 100);
        if (dist < 100 / 2)
        {
            currentHover = 1;
        }
        else
        {
            currentHover = -1;
        }
    }

    // constructor
    Circle(int x, int y, int d, int val) : x(x), y(y), d(d), val(val) {}

  protected:
    int x;
    int y;
    int d;   // diameter
    int val; //
    int currentHover = -1;
};

// helper function to set X foreground colour
enum Colour
{
    BLACK,
    WHITE
};
void setForeground(Colour c)
{
    if (c == BLACK)
    {
        XSetForeground(xinfo.display, xinfo.gc, BlackPixel(xinfo.display, xinfo.screen));
    }
    else
    {
        XSetForeground(xinfo.display, xinfo.gc, WhitePixel(xinfo.display, xinfo.screen));
    }
}

// A toggle button widget
class ToggleButton
{

  public:
    ToggleButton(int _x, int _y, void (*_toggleEvent)(bool))
    {
        x = _x;
        y = _y;
        toggleEvent = _toggleEvent;
        isOn = false;
        diameter = 100;
    }

    // the CONTROLLER
    int mouseClick(int mx, int my, int n)
    {
        float dist = sqrt(pow(mx - x - 50, 2) + pow(my - y - 50, 2));
        int space = (xinfo.width - 100 * n) / (2 + n - 1);
        int i = (x - space) / (space + 100);
        if (dist < diameter / 2)
        {
            clicked.push(i);
            return i;
        }
        return -1;
    }

    void setXY(int _x, int _y)
    {
        x = _x;
        y = _y;
    }

    int getX()
    {
        return x;
    }
    int getY()
    {
        return y;
    }

  private:
    // VIEW "essential geometry"
    int x;
    int y;
    int diameter;
    GC gc;

    // toggle event callback
    void (*toggleEvent)(bool);

    // the MODEL
    bool isOn;

    void toggle()
    {
        isOn = !isOn;
        toggleEvent(isOn);
    }
};

// placeholder method
void togglePause(bool isOn) {}

int FPS = 60;
/*
 * Function to put out a message on error exits.
 */
void error(string str)
{
    cerr << str << endl;
    exit(0);
}

// get microseconds
unsigned long now()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/*
 * Create a window
 */
void initX(int argc, char *argv[], XInfo &xinfo)
{

    /*
      * Display opening uses the DISPLAY  environment variable.
      * It can go wrong if DISPLAY isn't set, or you don't have permission.
      */
    xinfo.display = XOpenDisplay("");
    if (!xinfo.display)
    {
        error("Can't open display.");
    }

    /*
      * Find out some things about the display you're using.
      */
    // DefaultScreen is as macro to get default screen index
    int screen = DefaultScreen(xinfo.display);

    unsigned long white, black;
    white = XWhitePixel(xinfo.display, screen);
    black = XBlackPixel(xinfo.display, screen);

    xinfo.window = XCreateSimpleWindow(
        xinfo.display,                    // display where window appears
        DefaultRootWindow(xinfo.display), // window's parent in window tree
        10, 10,                           // upper left corner location
        800, 400,                         // size of the window
        5,                                // width of window's border
        black,                            // window border colour
        white);                           // window background colour
    xinfo.width = 800;
    xinfo.height = 400;

    // extra window properties like a window title
    XSetStandardProperties(
        xinfo.display, // display containing the window
        xinfo.window,  // window whose properties are set
        "a1-basic",    // window's title
        "A1",          // icon's title
        None,          // pixmap for the icon
        argv, argc,    // applications command line args
        None);         // size hints for the window

    /*
       * Put the window on the screen.
       */
    XMapRaised(xinfo.display, xinfo.window);

    XFlush(xinfo.display);

    // give server time to setup
    sleep(1);
}

// helper function to initially add circles and toggle bottons
void addCircles(list<Displayable *> &dList, list<ToggleButton *> &bList, XInfo &xinfo, int n)
{
    int space = (xinfo.width - 100 * n) / (2 + n - 1);
    for (int i = 0; i < n; i++)
    {
        dList.push_back(new Circle(space + space * i + 100 * i, xinfo.height / 2 - 50, 100, 1 + i));
        bList.push_back(new ToggleButton(space + space * i + 100 * i, xinfo.height / 2 - 50, &togglePause));
    }
}

/*
 * Function to repaint a display list
 */
void repaint(list<Displayable *> dList, XInfo &xinfo)
{
    list<Displayable *>::const_iterator begin = dList.begin();
    list<Displayable *>::const_iterator end = dList.end();

    XClearWindow(xinfo.display, xinfo.window);
    while (begin != end)
    {
        Displayable *d = *begin;
        d->paint(xinfo);
        begin++;
    }
    XFlush(xinfo.display);
}

int main(int argc, char *argv[])
{
    int n = 4;
    // get the number of buttons from args
    // (default to 4 if no args)
    if (argc > 1)
    {
        n = atoi(argv[1]);
    }
    n = max(1, min(n, 6));

    // create the Simon game object
    Simon simon = Simon(n, true);

    cout << "Playing with " << simon.getNumButtons() << " buttons." << endl;

    initX(argc, argv, xinfo);

    // create a simple graphics context
    GC gc = XCreateGC(xinfo.display, xinfo.window, 0, 0);
    int screen = DefaultScreen(xinfo.display);
    XSetForeground(xinfo.display, gc, BlackPixel(xinfo.display, screen));
    XSetBackground(xinfo.display, gc, WhitePixel(xinfo.display, screen));

    // load a larger font
    XFontStruct *font;
    font = XLoadQueryFont(xinfo.display, "12x24");
    XSetFont(xinfo.display, gc, font->fid);

    xinfo.gc = gc;
    xinfo.gc1 = XCreateGC(xinfo.display, xinfo.window, 0, 0);

    // list of Displayables
    list<Displayable *> dList;

    Text *score = new Text(50, 50, to_string(simon.getScore()));
    Text *hintText = new Text(50, 100, "Press SPACE to play");
    dList.push_back(score);
    dList.push_back(hintText);
    list<ToggleButton *> bList;
    addCircles(dList, bList, xinfo, n);

    // paint everything in the display list
    repaint(dList, xinfo);

    XFlush(xinfo.display);

    XEvent event;

    // time of last window paint
    unsigned long lastRepaint = 0;
    XSelectInput(xinfo.display, xinfo.window,
                 ButtonPressMask | KeyPressMask | StructureNotifyMask | PointerMotionMask); // select events

    XWindowAttributes w;
    XGetWindowAttributes(xinfo.display, xinfo.window, &w);
    int animationFrames = 25;
    int idleFrames = 20;

    double sinDiff = 0;
    int skipFrame = 40;
    // event loop
    while (true)
    {
        if (XPending(xinfo.display) > 0)
        {
            XNextEvent(xinfo.display, &event);

            switch (event.type)
            {

            // mouse button press
            case ButtonPress:
                int buttonClicked;
                for (ToggleButton *button : bList)
                {
                    int returnVal = button->mouseClick(event.xbutton.x, event.xbutton.y, n);
                    if (returnVal != -1)
                    {
                        buttonClicked = returnVal;
                    }
                }
                if (simon.getState() == Simon::HUMAN)
                {
                    // see if guess was correct
                    if (!simon.verifyButton(buttonClicked))
                    {
                        cout << "wrong" << endl;
                    }
                }

                // print the score
                //cout << "score: " << simon.getScore() << endl;
                break;

            case ConfigureNotify:
            {
                XConfigureEvent xce = event.xconfigure;

                /*  check whether the window has been resized. */

                if (xce.width != xinfo.width ||
                    xce.height != xinfo.height)
                {
                    xinfo.width = xce.width;
                    xinfo.height = xce.height;
                    for (int i = 0; i < n; i++)
                    {
                        dList.pop_back();
                        bList.pop_back();
                    }
                    addCircles(dList, bList, xinfo, n);
                }
            }
            break;

            case MotionNotify:
            {
                for (Displayable *dPointer : dList)
                {
                    {
                        if (dPointer->isCircle())
                        {
                            ((Circle *)dPointer)->mouseHover(event.xbutton.x, event.xbutton.y, n);
                        }
                    }
                }
            }
            break;

            case KeyPress: // any keypress
                KeySym key;
                char text[10];
                int i = XLookupString((XKeyEvent *)&event, text, 10, &key, 0);
                if (i == 1 && text[0] == 'q')
                {
                    XCloseDisplay(xinfo.display);
                    exit(0);
                }
                else if (i == 1 && text[0] == ' ')
                {
                    switch (simon.getState())
                    {
                    case Simon::START:
                        hintText->changeText("Press SPACE to play");
                        break;
                    // they won last round
                    // score is increased by 1, sequence length is increased by 1
                    case Simon::WIN:
                        hintText->changeText("You won! Press SPACE to continue.");
                        break;
                    // they lost last round
                    // score is reset to 0, sequence length is reset to 1
                    case Simon::LOSE:
                        hintText->changeText("You lose. Press SPACE to play again.");
                        break;
                    default:
                        // should never be any other state at this point ...
                        break;
                    }

                    // start new round with a new sequence
                    simon.newRound();
                    skipFrame = 40;
                    hintText->changeText("Watch what I do ...");
                    while (simon.getState() == Simon::COMPUTER)
                    {
                        clicked.push(simon.nextButton());
                    }
                }
                break;
            }
        }

        unsigned long end = now();

        score->changeText(to_string(simon.getScore()));
        if (end - lastRepaint > 1000000 / FPS)
        {

            // clear background
            XClearWindow(xinfo.display, xinfo.window);
            switch (simon.getState())
            {
            case Simon::START:
            case Simon::WIN:
            case Simon::LOSE:
            {
                // use a sine function to make the circles and toggle buttons move
                if (simon.getState() == Simon::WIN || simon.getState() == Simon::LOSE)
                {
                    if (skipFrame >= 0)
                    {
                        skipFrame--;
                        break;
                    }
                }
                double phase = 0;
                for (Displayable *dPointer : dList)
                {
                    phase += 1.57;
                    sinDiff += (3.14 / 240);
                    dPointer->setXY(dPointer->getX(), xinfo.height / 2 - 50 + 25 * sin(sinDiff + phase));
                }
            }
            break;
            default:
                for (Displayable *dPointer : dList)
                {
                    dPointer->setXY(dPointer->getX(), xinfo.height / 2 - 50);
                }
                break;
            }

            // paint everything in the display list
            repaint(dList, xinfo);
            if (clicked.size() > 0)
            {
                if (animationFrames >= 0 || idleFrames >= 0)
                {
                    if (animationFrames >= 0)
                    {
                        setForeground(BLACK);
                        int space = (xinfo.width - 100 * n) / (2 + n - 1);
                        XFillArc(xinfo.display, xinfo.window, xinfo.gc,
                                 space + space * clicked.front() + 100 * clicked.front(),
                                 xinfo.height / 2 - 50,
                                 100, 100,
                                 0, 360 * 64);
                        setForeground(WHITE);
                        XDrawArc(xinfo.display, xinfo.window, xinfo.gc,
                                 space + space * clicked.front() + 100 * clicked.front() + 2 * (25 - animationFrames),
                                 xinfo.height / 2 - 50 + 2 * (25 - animationFrames),
                                 100 - 4 * (25 - animationFrames), 100 - 4 * (25 - animationFrames),
                                 0, 360 * 64);
                        setForeground(BLACK);
                        animationFrames--;
                    }
                    else
                    {
                        idleFrames--;
                    }
                }
                else
                {
                    animationFrames = 25;
                    idleFrames = 20;
                    clicked.pop();
                    if (clicked.size() == 0 && simon.getState() == Simon::HUMAN)
                    {
                        hintText->changeText("Your turn ...");
                    }
                    else if (simon.getState() == Simon::LOSE)
                    {
                        hintText->changeText("You lose. Press SPACE to play again.");
                    }
                    else if (simon.getState() == Simon::WIN)
                    {
                        hintText->changeText("You won! Press SPACE to continue.");
                    }
                }
            }

            XFlush(xinfo.display);

            lastRepaint = now(); // remember when the paint happened
        }

        //IMPORTANT: sleep for a bit to let other processes work
        if (XPending(xinfo.display) == 0)
        {
            usleep(1000000 / FPS - (end - lastRepaint));
        }
    }
    XCloseDisplay(xinfo.display);
}
