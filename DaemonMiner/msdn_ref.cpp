/*
 * class MyService: public Service
{
protected:
    // The background thread that will be executing the application.
    // This handle is owned by this class.
    HANDLE appThread_;

public:
    // The exit code that will be set by the application thread on exit.
    DWORD exitCode_;

    // name - service name
    MyService(
        __in const std::wstring &name
    )
        : Service(name, true, true, false),
        appThread_(INVALID_HANDLE_VALUE),
        exitCode_(1) // be pessimistic
    { }

    ~MyService()
    {
        if (appThread_ != INVALID_HANDLE_VALUE) {
            CloseHandle(appThread_);
        }
    }

    virtual void onStart(
        __in DWORD argc,
        __in_ecount(argc) LPWSTR *argv)
    {
        setStateRunning();

        // start the thread that will execute the application
        appThread_ = CreateThread(NULL, 
            0, // do we need to change the stack size?
            &serviceMainFunction,
            (LPVOID)this,
            0, NULL);

        if (appThread_ == INVALID_HANDLE_VALUE) {
            log(WaSvcErrorSource.mkSystem(GetLastError(), 1, L"Failed to create the application thread:"),
                Logger::SV_ERROR);

            setStateStopped(1);
            return;
        }
    }

    virtual void onStop()
    {
        ... somehow tell the application thread to stop ...

        DWORD status = WaitForSingleObject(appThread_, INFINITE);
        if (status == WAIT_FAILED) {
            log(WaSvcErrorSource.mkSystem(GetLastError(), 1, L"Failed to wait for the application thread:"),
                Logger::SV_ERROR);
            // presumably exitCode_ already contains some reason at this point
        }

        // exitCode_ should be set by the application thread on exit
        setStateStopped(exitCode_);
    }
};
 */


/* ---------------------- SimpleService.hpp ---------------------- */
/*++
Copyright (c) 2016 Microsoft Corporation
https://blogs.msdn.microsoft.com/sergey_babkins_blog/2016/12/28/windows-service-wrapper-in-c/
--*/
/*
#pragma once
#include <string>
#include <datetimeapi.h>
#include <winsvc.h>

// -------------------- Service---------------------------------
// The general wrapper for running as a service.
// The subclasses need to define their virtual methods.

class DLLEXPORT Service
{
public:
	// The way the services work, there can be only one Service object
	// in the process. 
	Service(const std::wstring &name,
		bool canStop,
		bool canShutdown,
		bool canPauseContinue);


	virtual ~Service();

	// Run the service. Returns after the service gets stopped.
	// When the Service object gets started,
	// it will remember the instance pointer in the instance_ static
	// member, and use it in the callbacks.
	// The errors are reported back in err.
	void run(Erref &err);

	// Change the service state. Don't use it for SERVICE_STOPPED,
	// do that through the special versions.
	// Can be called only while run() is running.
	void setState(DWORD state);
	// The convenience versions.
	void setStateRunning()
	{
		setState(SERVICE_RUNNING);
	}
	void setStatePaused()
	{
		setState(SERVICE_PAUSED);
	}
	// The stopping is more compilcated: it also sets the exit code.
	// Which can be either general or a service-specific error code.
	// The success indication is the general code NO_ERROR.
	// Can be called only while run() is running.
	void setStateStopped(DWORD exitCode);
	void setStateStoppedSpecific(DWORD exitCode);

	// On the lengthy operations, periodically call this to tell the
	// controller that the service is not dead.
	// Can be called only while run() is running.
	void bump();

	// Can be used to set the expected length of long operations.
	// Also does the bump.
	// Can be called only while run() is running.
	void hintTime(DWORD msec);

	// Methods for the subclasses to override.
	// The base class defaults set the completion state, so the subclasses must
	// either call them at the end of processing (maybe after some wait, maybe
	// from another thread) or do it themselves.
	// The pending states (where applicable) will be set before these methods
	// are called.
	// onStart() is responsible for actually starting the application
	virtual void onStart(
		__in DWORD argc,
		__in_ecount(argc) LPWSTR *argv);
	virtual void onStop(); // sets the success exit code
	virtual void onPause();
	virtual void onContinue();
	virtual void onShutdown(); // calls onStop()

protected:
	// The callback for the service start.
	static void WINAPI serviceMain(
		__in DWORD argc,
		__in_ecount(argc) LPWSTR *argv);
	// The callback for the requests.
	static void WINAPI serviceCtrlHandler(DWORD ctrl);

	// the internal version that expects the caller to already hold statusCr_
	void setStateL(DWORD state);

protected:
	static Service *instance_;

	std::wstring name_; // service name

	Critical statusCr_; // protects the status setting
	SERVICE_STATUS_HANDLE statusHandle_; // handle used to report the status
	SERVICE_STATUS status_; // the current status

	Critical errCr_; // protects the error handling
	Erref err_; // the collected errors

private:
	Service();
	Service(const Service &);
	void operator=(const Service &);
};
*/
/* ---------------------- SimpleService.cpp ---------------------- */
/*++
Copyright (c) 2016 Microsoft Corporation
--*/
/*
// ... use the right includes ...

static ErrorMsg::MuiSource ServiceErrorSource(L"Service", NULL);

// -------------------- Service---------------------------------

Service *Service::instance_;

Service::Service(const wstring &name,
	bool canStop,
	bool canShutdown,
	bool canPauseContinue
) :
	name_(name), statusHandle_(NULL)
{

	// The service runs in its own process.
	status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	// The service is starting.
	status_.dwCurrentState = SERVICE_START_PENDING;

	// The accepted commands of the service.
	status_.dwControlsAccepted = 0;
	if (canStop)
		status_.dwControlsAccepted |= SERVICE_ACCEPT_STOP;
	if (canShutdown)
		status_.dwControlsAccepted |= SERVICE_ACCEPT_SHUTDOWN;
	if (canPauseContinue)
		status_.dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;

	status_.dwWin32ExitCode = NO_ERROR;
	status_.dwServiceSpecificExitCode = 0;
	status_.dwCheckPoint = 0;
	status_.dwWaitHint = 0;
}

Service::~Service()
{ }

void Service::run(Erref &err)
{
	err_.reset();
	instance_ = this;

	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ (LPWSTR)name_.c_str(), serviceMain },
	{ NULL, NULL }
	};

	if (!StartServiceCtrlDispatcher(serviceTable)) {
		err_ = ServiceErrorSource.mkMuiSystem(GetLastError(), EPEM_SERVICE_DISPATCHER_FAIL, name_.c_str());
	}

	err = err_.copy();
}

void WINAPI Service::serviceMain(
	__in DWORD argc,
	__in_ecount(argc) LPWSTR *argv)
{
	REAL_ASSERT(instance_ != NULL);

	// Register the handler function for the service
	instance_->statusHandle_ = RegisterServiceCtrlHandler(
		instance_->name_.c_str(), serviceCtrlHandler);
	if (instance_->statusHandle_ == NULL)
	{
		instance_->err_.append(ServiceErrorSource.mkMuiSystem(GetLastError(),
			EPEM_SERVICE_HANDLER_REGISTER_FAIL, instance_->name_.c_str()));
		instance_->setStateStoppedSpecific(EPEM_SERVICE_HANDLER_REGISTER_FAIL);
		return;
	}

	// Start the service.
	instance_->setState(SERVICE_START_PENDING);
	instance_->onStart(argc, argv);
}

void WINAPI Service::serviceCtrlHandler(DWORD ctrl)
{
	switch (ctrl)
	{
	case SERVICE_CONTROL_STOP:
		if (instance_->status_.dwControlsAccepted & SERVICE_ACCEPT_STOP) {
			instance_->setState(SERVICE_STOP_PENDING);
			instance_->onStop();
		}
		break;
	case SERVICE_CONTROL_PAUSE:
		if (instance_->status_.dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) {
			instance_->setState(SERVICE_PAUSE_PENDING);
			instance_->onPause();
		}
		break;
	case SERVICE_CONTROL_CONTINUE:
		if (instance_->status_.dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) {
			instance_->setState(SERVICE_CONTINUE_PENDING);
			instance_->onContinue();
		}
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		if (instance_->status_.dwControlsAccepted & SERVICE_ACCEPT_SHUTDOWN) {
			instance_->setState(SERVICE_STOP_PENDING);
			instance_->onShutdown();
		}
		break;
	case SERVICE_CONTROL_INTERROGATE:
		SetServiceStatus(instance_->statusHandle_, &instance_->status_);
		break;
	default:
		break;
	}
}

void Service::setState(DWORD state)
{
	ScopeCritical sc(statusCr_);

	setStateL(state);
}

void Service::setStateL(DWORD state)
{
	status_.dwCurrentState = state;
	status_.dwCheckPoint = 0;
	status_.dwWaitHint = 0;
	SetServiceStatus(statusHandle_, &status_);
}

void Service::setStateStopped(DWORD exitCode)
{
	ScopeCritical sc(statusCr_);

	status_.dwWin32ExitCode = exitCode;
	setStateL(SERVICE_STOPPED);
}

void Service::setStateStoppedSpecific(DWORD exitCode)
{
	ScopeCritical sc(statusCr_);

	status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	status_.dwServiceSpecificExitCode = exitCode;
	setStateL(SERVICE_STOPPED);
}

void Service::bump()
{
	ScopeCritical sc(statusCr_);

	++status_.dwCheckPoint;
	::SetServiceStatus(statusHandle_, &status_);
}

void Service::hintTime(DWORD msec)
{
	ScopeCritical sc(statusCr_);

	++status_.dwCheckPoint;
	status_.dwWaitHint = msec;
	::SetServiceStatus(statusHandle_, &status_);
	status_.dwWaitHint = 0; // won't apply after the next update
}

void Service::onStart(
	__in DWORD argc,
	__in_ecount(argc) LPWSTR *argv)
{
	setState(SERVICE_RUNNING);
}
void Service::onStop()
{
	setStateStopped(NO_ERROR);
}
void Service::onPause()
{
	setState(SERVICE_PAUSED);
}
void Service::onContinue()
{
	setState(SERVICE_RUNNING);
}
void Service::onShutdown()
{
	onStop();
}

*/