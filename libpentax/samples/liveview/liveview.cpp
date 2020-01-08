
// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------


// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#include <wx/mstream.h>
#include <wx/dcbuffer.h>

#include <ricoh_camera_sdk.hpp>

#include <mutex>

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

using namespace Ricoh::CameraController;

// ----------------------------------------------------------------------------
// resources
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp
{
public:
    // override base class virtuals
    // ----------------------------

    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    virtual bool OnInit() override;
    virtual bool OnExceptionInMainLoop() override;
};

// Define a panel for image view
class MyImagePanel : public wxPanel
{
public:
    // ctor(s)
    MyImagePanel(wxFrame* parent);

    void paintNow();
    void setImageData(const std::shared_ptr<const unsigned char> data, int length);
    void clearImageData();

private:
    std::recursive_mutex imageMutex_;
    wxImage image_;
    std::shared_ptr<const unsigned char> data_;

    // event handlers (these functions should _not_ be virtual)
    void PaintEvent(wxPaintEvent & evt);
    void OnSize(wxSizeEvent& event);

    void DoRefresh(wxCommandEvent& event);

    void render(wxDC& dc);

    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()

};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
    // ctor(s)
    MyFrame(const wxString& title);

private:
    // event handlers (these functions should _not_ be virtual)
    void OnClose(wxCloseEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnConnectBtn(wxCommandEvent& event);
    void OnDisconnectBtn(wxCommandEvent& event);
    void OnStartLVBtn(wxCommandEvent& event);
    void OnStopLVBtn(wxCommandEvent& event);
    void OnStartCaptureBtn(wxCommandEvent& event);
    void OnStartCaptureWithoutFocusBtn(wxCommandEvent& event);
    void OnStopCaptureBtn(wxCommandEvent& event);
    void DoUpdateStatus(wxCommandEvent& event);

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();

    void showActionResult(std::string name, Response response);
};

// Define camera event listener
class EventListener : public CameraEventListener {
public:

    void imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                    const std::shared_ptr<const CameraImage>& image) override;

    void captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                         const std::shared_ptr<const Capture>& capture) override;

    void deviceDisconnected(const std::shared_ptr<const CameraDevice>& sender,
                            DeviceInterface inf) override;

    virtual void liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender,
                                      const std::shared_ptr<const unsigned char>& liveViewFrame,
                                      uint64_t frameSize) override;
};

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

static std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices;
static std::shared_ptr<CameraDevice> camera;

MyFrame* frame;
MyImagePanel* imagePanel;

constexpr int imageWidth = 720;
constexpr int imageHeight = 480;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// Define original event
wxDEFINE_EVENT(MY_REFRESH_EVENT, wxCommandEvent);
wxDEFINE_EVENT(MY_UPDATE_STATUS_EVENT, wxCommandEvent);

// IDs for the controls and the menu commands
enum
{
    // menu items
    Minimal_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    Minimal_About = wxID_ABOUT,

    wxID_CONNECT,
    wxID_DISCONNECT,
    wxID_START_LV,
    wxID_STOP_LV,
    wxID_START_CAPTURE,
    wxID_START_CAPTURE_WITHOUT_FOCUS,
    wxID_STOP_CAPTURE,
    wxID_MY_REFRESH,
    wxID_MY_UPDATE_STATUS
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWidgets
// ----------------------------------------------------------------------------

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_CLOSE(MyFrame::OnClose)
    EVT_MENU(Minimal_Quit,            MyFrame::OnQuit)
    EVT_MENU(Minimal_About,           MyFrame::OnAbout)
    EVT_BUTTON(wxID_CONNECT,          MyFrame::OnConnectBtn)
    EVT_BUTTON(wxID_DISCONNECT,       MyFrame::OnDisconnectBtn)
    EVT_BUTTON(wxID_START_LV,         MyFrame::OnStartLVBtn)
    EVT_BUTTON(wxID_STOP_LV,          MyFrame::OnStopLVBtn)
    EVT_BUTTON(wxID_START_CAPTURE,    MyFrame::OnStartCaptureBtn)
    EVT_BUTTON(wxID_START_CAPTURE_WITHOUT_FOCUS, MyFrame::OnStartCaptureWithoutFocusBtn)
    EVT_BUTTON(wxID_STOP_CAPTURE,     MyFrame::OnStopCaptureBtn)
    EVT_COMMAND(wxID_MY_UPDATE_STATUS, MY_UPDATE_STATUS_EVENT, MyFrame::DoUpdateStatus)
wxEND_EVENT_TABLE()

BEGIN_EVENT_TABLE(MyImagePanel, wxPanel)
    EVT_PAINT(MyImagePanel::PaintEvent) // catch paint events
    EVT_SIZE(MyImagePanel::OnSize)      //Size event
    EVT_COMMAND(wxID_MY_REFRESH, MY_REFRESH_EVENT, MyImagePanel::DoRefresh)
END_EVENT_TABLE()

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
wxIMPLEMENT_APP(MyApp);

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
    // call the base class initialization method, currently it only parses a
    // few common command-line options but it could be do more in the future
    if (!wxApp::OnInit()) {
        return false;
    }

    // create the main application window
    frame = new MyFrame("RICOH Camera USB SDK for C++ LiveView Sample Application");

    // and show it (the frames, unlike simple controls, are not shown when
    // created initially)
    frame->Show(true);

    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned false here, the
    // application would exit immediately.
    return true;
}

bool MyApp::OnExceptionInMainLoop()
{
    bool canContinue = false;

    std::string message = "Unexpected error has occurred";
    try {
        // Rethrow the current exception.
        throw;
    } catch (const std::runtime_error& e) {
        std::string detail = std::string(e.what());
        message += ": " + detail;
        if (detail.find("Not Supported") != std::string::npos) {
            canContinue = true;
        }
    } catch ( ... ) {
    }

    frame->SetStatusText(message + (canContinue?".":".   This proram wil terminate."));

    if (!canContinue) {
        if (camera != nullptr) {
            camera->disconnect(DeviceInterface::USB);
        }
    }

    return canContinue;
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

// frame constructor
MyFrame::MyFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title)
{
    // create a menu bar
    wxMenu *fileMenu = new wxMenu;

    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(Minimal_About, "&About\tF1", "Show about dialog");
    fileMenu->Append(Minimal_Quit, "E&xit\tAlt-X", "Quit this program");

    // now append the freshly created menu to the menu bar...
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");

    // ... and attach this menu bar to the frame
    SetMenuBar(menuBar);

    // create a status bar just for fun (by default with 1 pane only)
    CreateStatusBar(1);
    SetStatusText("Welcome to LiveView Sample Application!");

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // create buttons
    wxBoxSizer* sizerBttn = new wxBoxSizer(wxHORIZONTAL);
    wxButton* connectBtn = new wxButton(this, wxID_CONNECT, "Connect");
    sizerBttn->Add(connectBtn, 1, wxEXPAND, 0);
    wxButton* disconnectBtn = new wxButton(this, wxID_DISCONNECT, "Disconnect");
    sizerBttn->Add(disconnectBtn, 1, wxEXPAND, 0);
    wxButton* startLVBtn = new wxButton(this, wxID_START_LV, "StartLiveView");
    sizerBttn->Add(startLVBtn, 1, wxEXPAND, 0);
    wxButton* stopLVBtn = new wxButton(this, wxID_STOP_LV, "StopLiveView");
    sizerBttn->Add(stopLVBtn, 1, wxEXPAND, 0);
    wxButton* startCaptureBtn = new wxButton(this, wxID_START_CAPTURE, "StartCapture");
    sizerBttn->Add(startCaptureBtn, 1, wxEXPAND, 0);
    wxButton* startCaptureWithoutFocusBtn = new wxButton(this, wxID_START_CAPTURE_WITHOUT_FOCUS,
                                             "StartCaptureWithoutFocus");
    sizerBttn->Add(startCaptureWithoutFocusBtn, 2, wxEXPAND, 0);
    wxButton* stopCaptureBtn = new wxButton(this, wxID_STOP_CAPTURE,
                                            "StopCapture");
    sizerBttn->Add(stopCaptureBtn, 1, wxEXPAND, 0);

    sizer->Add(sizerBttn, 1, wxFIXED_MINSIZE, 0);

    imagePanel = new MyImagePanel(this);
    imagePanel->SetSize(imageWidth, imageHeight);

    sizer->Add(imagePanel, 9, wxFIXED_MINSIZE | wxALIGN_CENTER, 0);

    SetSizerAndFit(sizer);

    SetMinSize(GetSize());
    SetMaxSize(GetSize());

    Centre();
}


// event handlers

void MyFrame::OnClose(wxCloseEvent& event)
{
    if (camera != nullptr) {
        camera->disconnect(DeviceInterface::USB);
    }
    event.Skip();
}

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    // true is to force the frame to close
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(wxString::Format
                 (
                    "RICOH Camera USB SDK for C++\n"
                    "LiveView Sample Application\n"
                    "\n"
                    "Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.\n"
                    "\n\n"
                    "This is running under %s.",
                    wxVERSION_STRING,
                    wxGetOsDescription()
                 ),
                 "About This Application",
                 wxOK | wxICON_INFORMATION,
                 this);
}

void MyFrame::OnConnectBtn(wxCommandEvent& WXUNUSED(event))
{
    SetStatusText("Connecting...");

    detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
    if (detectedCameraDevices.size() > 0) {
        camera = detectedCameraDevices[0];
        if (camera->getEventListeners().size() == 0) {
            std::shared_ptr<CameraEventListener> listener =
                std::make_shared<EventListener>();
            camera->addEventListener(listener);
        }
        Response response = camera->connect(DeviceInterface::USB);
        if (response.getResult() == Result::Ok) {
            std::string message = "Connected. Model:" + camera->getModel() +
                                  ", SerialNumber:" + camera->getSerialNumber();
            SetStatusText(message);
        } else {
            SetStatusText("Connection is failed.");
        }
    } else {
        SetStatusText("Device has not been found.");
    }
}

void MyFrame::OnDisconnectBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("Disconnecting...");

    Response response = camera->disconnect(DeviceInterface::USB);
    if (response.getResult() == Result::Ok) {
        imagePanel->clearImageData();
        SetStatusText("Disconnected.");
    } else {
        SetStatusText("Disconnection is failed.");
    }
}

void MyFrame::OnStartLVBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("LiveView is starting...");

    Response response = camera->startLiveView();
    if (response.getResult() == Result::Ok) {
        std::string StatusBar = "LiveView has been started.";
        SetStatusText(StatusBar);
    } else {
        showActionResult("StartLiveView", response);
    }
}

void MyFrame::OnStopLVBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("LiveView is stopping...");

    Response response = camera->stopLiveView();
    if (response.getResult() == Result::Ok) {
        SetStatusText("LiveView has been stopped.");
        imagePanel->clearImageData();
    } else {
        showActionResult("StopLiveView", response);
    }
}

void MyFrame::OnStartCaptureBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("StartCapture is executing...");

    StartCaptureResponse response = camera->startCapture();

    showActionResult("StartCapture", response);
}

void MyFrame::OnStartCaptureWithoutFocusBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("StartCapture without focus is executing...");

    StartCaptureResponse response = camera->startCapture(false);

    showActionResult("StartCapture without focus", response);
}

void MyFrame::OnStopCaptureBtn(wxCommandEvent& WXUNUSED(event))
{
    if (camera == nullptr) {
        return;
    }

    SetStatusText("StopCapture.");

    Response response = camera->stopCapture();

    if (response.getResult() == Result::Ok) {
        SetStatusText("Capture has been stopped.");
    } else {
        showActionResult("StopCapture", response);
    }
}

void MyFrame::DoUpdateStatus(wxCommandEvent& event)
{
    SetStatusText(event.GetString());
}

void MyFrame::showActionResult(std::string name, Response response)
{
    if (response.getResult() == Result::Ok) {
        SetStatusText(name + " result: OK.");
        return;
    }

    const std::vector<std::shared_ptr<Error>> errors = response.getErrors();
    if (errors.size() == 0) {
        SetStatusText(name + " result: Error.");
    }
    else {
        std::string code = std::to_string(static_cast<int>(errors[0]->getCode()));
        SetStatusText(name + " result: Error, " +
            "Code: " + code + ", Message: " + errors[0]->getMessage());
    }
}

// ----------------------------------------------------------------------------
// image panel
// ----------------------------------------------------------------------------

// panel constructor
MyImagePanel::MyImagePanel(wxFrame* parent) : wxPanel(parent)
{
    wxImage::AddHandler(new wxJPEGHandler);
}

// Called by the system of by wxWidgets when the panel needs
// to be redrawn. You can also trigger this call by
// calling Refresh()/Update().
void MyImagePanel::PaintEvent(wxPaintEvent & evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

// Alternatively, you can use a clientDC to paint on the panel
// at any time. Using this generally does not free you from
// catching paint events, since it is possible that e.g. the window
// manager throws away your drawing when the window comes to the
// background, and expects you will redraw it when the window comes
// back (by sending a paint event).
void MyImagePanel::paintNow()
{
    wxClientDC dc(this);
    render(dc);
}

// Here we do the actual rendering. I put it in a separate
// method so that it can work no matter what type of DC
// (e.g. wxPaintDC or wxClientDC) is used.
void MyImagePanel::render(wxDC&  dc)
{
    std::lock_guard<std::recursive_mutex> lock(imageMutex_);

    if (!image_.IsOk()) {
        return;
    }

    int neww, newh;
    dc.GetSize( &neww, &newh );

    wxBitmap resized = image_.Scale(neww, newh);

    // depending on your system you may need to look at double-buffered dcs
    if (this->IsDoubleBuffered()) {
        dc.DrawBitmap(resized, 0, 0, false);
    } else {
        wxBufferedDC dcs(&dc, wxSize(neww, newh));
        dcs.DrawBitmap(resized, 0, 0, false);
    }
}

// Here we call refresh to tell the panel to draw itself again.
// So when the user resizes the image panel the image should be resized too.
void MyImagePanel::OnSize(wxSizeEvent& evt)
{
    Refresh(false);
    //skip the event.
    evt.Skip();
}

void MyImagePanel::DoRefresh(wxCommandEvent& event)
{
    Refresh(false);
}

void MyImagePanel::setImageData(const std::shared_ptr<const unsigned char> data, int length)
{
    std::lock_guard<std::recursive_mutex> lock(imageMutex_);

    data_ = data;
    wxMemoryInputStream mis(data.get(), length);
    image_.LoadFile(mis, wxBITMAP_TYPE_JPEG);
}

void MyImagePanel::clearImageData()
{
    std::lock_guard<std::recursive_mutex> lock(imageMutex_);

    if (!image_.IsOk()) {
        return;
    }
    data_ = nullptr;
    image_.Clear();
    Refresh(false);
}

// ----------------------------------------------------------------------------
// camera event listener
// ----------------------------------------------------------------------------

void EventListener::imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                               const std::shared_ptr<const CameraImage>& image)
{
    wxCommandEvent* ev = new wxCommandEvent(MY_UPDATE_STATUS_EVENT, wxID_MY_UPDATE_STATUS);
    std::string message = "Image(" + image->getName() + ") has been added.";
    ev->SetString(message);
    wxQueueEvent(frame, ev);
}

void EventListener::captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                                    const std::shared_ptr<const Capture>& capture)
{
    // wxCommandEvent* ev = new wxCommandEvent(MY_UPDATE_STATUS_EVENT, wxID_MY_UPDATE_STATUS);
    // std::string message = "Capture(" + capture->getId() + ") has been completed.";
    // ev->SetString(message);
    // wxQueueEvent(frame, ev);
}

void EventListener::deviceDisconnected(const std::shared_ptr<const CameraDevice>& sender,
                                       DeviceInterface inf)
{
    wxCommandEvent* ev = new wxCommandEvent(MY_UPDATE_STATUS_EVENT, wxID_MY_UPDATE_STATUS);
    ev->SetString("Device has been disconnected.");
    wxQueueEvent(frame, ev);
}

void EventListener::liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender,
                                         const std::shared_ptr<const unsigned char>& liveViewFrame,
                                         uint64_t frameSize)
{
    imagePanel->setImageData(liveViewFrame, frameSize);
    wxQueueEvent(imagePanel, new wxCommandEvent(MY_REFRESH_EVENT, wxID_MY_REFRESH));
}
