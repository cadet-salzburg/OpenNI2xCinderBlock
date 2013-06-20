#include "OpenNI2xWrapper.h"


using namespace std;
using namespace ci;
using namespace ci::app;



OpenNI2xWrapper::OpenNI2xWrapper(void)
{
}

OpenNI2xWrapper::~OpenNI2xWrapper(void)
{
	std::cout << "over and out" << std::endl;
}

OpenNI2xWrapper& OpenNI2xWrapper::getInstance()
{
	static OpenNI2xWrapper instance;
	return instance;
}

void OpenNI2xWrapper::onDeviceStateChanged(const openni::DeviceInfo* pInfo, openni::DeviceState state) 
{
	printf("Device \"%s\" error state changed to %d\n", pInfo->getUri(), state);
}

void OpenNI2xWrapper::onDeviceConnected(const openni::DeviceInfo* pInfo)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	if(strcmp(pInfo->getName(),"oni File"))			// only autostart device if it is not an openend recording otherwise recording is opened twice
	{
		cout << "Device " << pInfo->getUri() << " connected" << endl;
		openni::OpenNI::enumerateDevices(&m_DeviceInfoList);			// count connected devices
		startDevice(pInfo->getUri());
	}
}

void OpenNI2xWrapper::onDeviceDisconnected(const openni::DeviceInfo* pInfo)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	printf("Device \"%s\" disconnected\n", pInfo->getUri());
	stopDevice(pInfo->getUri());
	openni::OpenNI::enumerateDevices(&m_DeviceInfoList);			// count connected devices
}

bool OpenNI2xWrapper::init(bool bUseUserTracking)
{
	openni::Status rc = openni::STATUS_OK;
	nite::Status niterc = nite::STATUS_OK;

	rc = openni::OpenNI::initialize();
	std::cout << "OpenNI: Init OpenNI " << openni::OpenNI::getVersion().major <<  "." << openni::OpenNI::getVersion().minor << std::endl;
	if(rc != openni::STATUS_OK)
	{
		std::cout << "OpenNI: Couldn't initialize OpenNI, check for installs, environments vars, dlls and if you have set ..\\..\\bin as working dir in visual studio " << openni::OpenNI::getExtendedError() << std::endl;
		return false;
	}
		

	m_bUserTrackingInitizialized=false;
	
	if(bUseUserTracking)
	{
		niterc = nite::NiTE::initialize();
		std::cout << "OpenNI: Init Nite " << nite::NiTE::getVersion().major <<  "." << nite::NiTE::getVersion().minor << std::endl;
	
		if(niterc != nite::STATUS_OK)
		{
			std::cout << "OpenNI: Couldn't initialize NITE SDK" << std::endl;
			return false;
		}
		m_bUserTrackingInitizialized = true;
	}

	//register device connection state listeners
	openni::OpenNI::addDeviceConnectedListener(this);
	openni::OpenNI::addDeviceDisconnectedListener(this);
	openni::OpenNI::addDeviceStateChangedListener(this);

	openni::OpenNI::enumerateDevices(&m_DeviceInfoList);			// count connected devices
	std::cout << "OpenNI: Number of connected OpenNI devices: " << m_DeviceInfoList.getSize() << std::endl;

	return true;
}

int16_t OpenNI2xWrapper::startDevice(string uri, bool bHasRGBStream, bool bHasDepthStream, bool bHasUserTracker, bool bHasIRStream)
{
	for(int i=0; i<m_DeviceInfoList.getSize(); i++)
	{
		if(!strcmp(m_DeviceInfoList[i].getUri(), uri.c_str()))
			return startDevice(i, bHasRGBStream, bHasDepthStream, bHasUserTracker, bHasIRStream);
	}
	return -1;
}

int16_t OpenNI2xWrapper::startDevice(uint16_t iDeviceNumber, bool bHasRGBStream, bool bHasDepthStream, bool bHasUserTracker, bool bHasIRStream)
{
	int iDeviceID=-1;
	openni::Status rc = openni::STATUS_OK;
	nite::Status niterc = nite::STATUS_OK;

	if(iDeviceNumber>=m_DeviceInfoList.getSize())
	{
		std::cout << "OpenNI: Failure device number: " << m_DeviceInfoList.getSize() << " not detected --> check drivers and device manager" << std::endl;
		return -1;
	}

	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	std::shared_ptr<OpenNIDevice> device;

	int16_t registeredID=getRegisteredDeviceNumberForURI(m_DeviceInfoList[iDeviceNumber].getUri());
	if(registeredID==-1)
	{
		OpenNIDevice* tmp = new OpenNIDevice();
		std::shared_ptr<OpenNIDevice> deviceTmp = std::shared_ptr<OpenNIDevice>(tmp);
		device = deviceTmp;
		device->m_Uri = m_DeviceInfoList[iDeviceNumber].getUri();
	}
	else
		device = m_Devices[registeredID];
	if(! device->m_Device.isValid())
	{
		rc = device->m_Device.open(device->m_Uri.c_str());
		if (rc != openni::STATUS_OK)
		{
			printf("OpenNI: Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
			return -1;
		}
	}
	
	if(registeredID==-1)
	{
		device->m_bRGBStreamActive = bHasRGBStream;
		device->m_bDepthStreamActive = bHasDepthStream;
		device->m_bUserStreamActive = bHasUserTracker && m_bUserTrackingInitizialized;
		device->m_bIRStreamActive = bHasIRStream;
		m_Devices.push_back(device);	// push in list of all running devices
	}

	if(startStreams( iDeviceNumber, device->m_bRGBStreamActive, device->m_bDepthStreamActive, device->m_bUserStreamActive, device->m_bIRStreamActive))
		return iDeviceID;
	else
		return -1;
}

void OpenNI2xWrapper::stopDevice(std::string uri)
{
	for(int i=0; i<m_DeviceInfoList.getSize(); i++)
	{
		if(!strcmp(m_DeviceInfoList[i].getUri(), uri.c_str()))
		{	
			stopDevice(i);
			return;
		}
	}
}

void OpenNI2xWrapper::stopDevice(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
		return;
	
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	// lock for correct asynchronous calls of update and stop
	
	m_Devices[iDeviceNumber]->m_bIsRunning=false;
	// not sure if I have to care for cleanup of the streams running, as they are part of the device
	if(m_Devices[iDeviceNumber]->m_pUserTracker != nullptr)
	{
		m_Devices[iDeviceNumber]->m_UserTrackerFrame.release();
		m_Devices[iDeviceNumber]->m_pUserTracker->destroy();
		m_Devices[iDeviceNumber]->m_pUserTracker=nullptr;
	}

	if (m_Devices[iDeviceNumber]->m_bDepthStreamActive) 
	{
		m_Devices[iDeviceNumber]->m_DepthStream.stop();
		m_Devices[iDeviceNumber]->m_DepthStream.destroy();
	}
	if (m_Devices[iDeviceNumber]->m_bRGBStreamActive)
	{
		m_Devices[iDeviceNumber]->m_RGBStream.stop();
		m_Devices[iDeviceNumber]->m_RGBStream.destroy();
	}
	if (m_Devices[iDeviceNumber]->m_bIRStreamActive)
	{
		m_Devices[iDeviceNumber]->m_IRStream.stop();
		m_Devices[iDeviceNumber]->m_IRStream.destroy();
	}
		
	m_Devices[iDeviceNumber]->m_Device.close();
	//
	//// cleanup memory
	if(m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr != nullptr)
	{
		delete m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr;
		m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr = nullptr;
	}
}

bool OpenNI2xWrapper::startStreams(uint16_t iDeviceNumber, bool bHasRGBStream, bool bHasDepthStream, bool bHasUserTracker, bool hasIRStream)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	// lock for correct asynchronous calls of update and stop

	int streamCount=0;
	std::shared_ptr<OpenNIDevice> device = m_Devices[iDeviceNumber];
	device->m_pStreams = new openni::VideoStream*[2];

	
	if(m_bUserTrackingInitizialized && bHasUserTracker)
	{
		if( device->m_pUserTracker==nullptr)
		{
			cout << "Start User Tracker" << endl;
			device->m_pUserTracker =   new nite::UserTracker();
		
			if(device->m_pUserTracker->create(&(device->m_Device)) != nite::STATUS_OK)
			{
				device->m_bUserStreamActive=false;
				device->m_UserTrackerFrame.release();
				device->m_pUserTracker->destroy();
				device->m_pUserTracker=nullptr;
				std::cout << "OpenNI: Couldn't create UserStream\n" << std::endl;
			}
		}
	}
	
	if(bHasRGBStream)
	{
		if(device->m_RGBStream.create(device->m_Device, openni::SENSOR_COLOR) == openni::STATUS_OK && device->m_RGBStream.start() == openni::STATUS_OK)
		{
			openni::VideoMode colorVideoMode = device->m_RGBStream.getVideoMode();
			device->m_iRgbImgWidth = colorVideoMode.getResolutionX();
			device->m_iRgbImgHeight = colorVideoMode.getResolutionY();
			device->m_pStreams[streamCount++] = &device->m_RGBStream;
		}
		else
		{
			device->m_bRGBStreamActive=false;
			std::cout << "OpenNI: Couldn't start color stream: " << openni::OpenNI::getExtendedError() << std::endl;
		}
	}
	if(bHasDepthStream)
	{
		if(device->m_DepthStream.create(device->m_Device, openni::SENSOR_DEPTH) == openni::STATUS_OK && device->m_DepthStream.start() == openni::STATUS_OK)
		{
			openni::VideoMode depthVideoMode = device->m_DepthStream.getVideoMode();
			device->m_iDepthImgWidth = depthVideoMode.getResolutionX();
			device->m_iDepthImgHeight = depthVideoMode.getResolutionY();
			device->m_pStreams[streamCount++] = &device->m_DepthStream;
		}
		else
		{
			device->m_bDepthStreamActive=false;
			std::cout << "OpenNI: Couldn't start depth stream: " << openni::OpenNI::getExtendedError() << std::endl;
		}
	}
	if(hasIRStream)
	{
		if(device->m_IRStream.create(device->m_Device, openni::SENSOR_IR) == openni::STATUS_OK && device->m_IRStream.start() == openni::STATUS_OK)
		{
			openni::VideoMode irVideoMode = device->m_IRStream.getVideoMode();
			device->m_iIRImgWidth = irVideoMode.getResolutionX();
			device->m_iIRImgHeight = irVideoMode.getResolutionY();
			device->m_pStreams[streamCount++] = &device->m_IRStream;
		}
		else
		{
			device->m_bIRStreamActive=false;
			std::cout << "OpenNI: Couldn't start ir stream: " << openni::OpenNI::getExtendedError() << std::endl;
		}
	}

	setDepthColorImageAlignment(iDeviceNumber, true);
	setBackgroundSubtraction(iDeviceNumber, false);

	// freezes the app
	//setDepthColorSync(iDeviceNumber, true);
	
	device->m_bIsRunning=true;
	return true;
}

int16_t OpenNI2xWrapper::getRegisteredDeviceNumberForURI(std::string uri)
{
	for(uint16_t i=0; i<m_Devices.size(); i++)
	{
		if(m_Devices[i]->m_Uri == uri)
			return i;
	}
	
	//std::cout << "No device registered in system with the specified URI" << std::endl;
	return -1;	// no device with this uri found connected
}

uint16_t OpenNI2xWrapper::getDevicesConnected()
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	
	return m_DeviceInfoList.getSize();
}

uint16_t OpenNI2xWrapper::getDevicesInitialized()
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	
	return m_Devices.size();
}

bool OpenNI2xWrapper::isDeviceRunning(uint16_t iDeviceNumber)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << "not initialized or running" << endl;
		return false;
	}
	return m_Devices[iDeviceNumber]->m_bIsRunning;
}

bool OpenNI2xWrapper::shutdown()
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	
	
	std::cout << "OpenNI: Shutdown" << std::endl;

	for(uint16_t i=0; i<m_Devices.size(); i++)
		stopDevice(i);
	m_Devices.clear();

	nite::NiTE::shutdown();
	openni::OpenNI::shutdown();
	
	
	

	return true;
}

bool OpenNI2xWrapper::resetDevice(uint16_t iDeviceNumber)
{

	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << "not initialized or running" << endl;
		return false;
	}
	
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);

	std::cout << "OpenNI: Reset Device " << iDeviceNumber << " " << std::endl;
	stopDevice(iDeviceNumber);
	startDevice(iDeviceNumber);

	return true;
}

void OpenNI2xWrapper::updateDevice(uint16_t iDeviceNumber)
{
	int streamReady=-1;
	
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	


	if(iDeviceNumber>=m_Devices.size() || !m_Devices[iDeviceNumber]->m_bIsRunning)
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}

	// non-blocking wait --> return if not both streams are ready to process
	if(openni::OpenNI::waitForAnyStream(m_Devices[iDeviceNumber]->m_pStreams, 1, &streamReady, 0) == openni::STATUS_TIME_OUT ||
	openni::OpenNI::waitForAnyStream((m_Devices[iDeviceNumber]->m_pStreams+1), 1, &streamReady, 0) == openni::STATUS_TIME_OUT)
	{
		//cout << "OpenNI: Device " << iDeviceNumber << " timed out" << endl;
		return; 
	}

	
	// user tracking / skeleton tracking
	if(m_Devices[iDeviceNumber]->m_bUserStreamActive && m_Devices[iDeviceNumber]->m_pUserTracker->isValid())
	{
		nite::Status niteRc = m_Devices[iDeviceNumber]->m_pUserTracker->readFrame(&m_Devices[iDeviceNumber]->m_UserTrackerFrame);
		m_Devices[iDeviceNumber]->m_DepthFrame = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getDepthFrame();
		
		if(niteRc == nite::STATUS_OK)
		{
			const nite::UserMap& userLabels = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUserMap();
			m_Devices[iDeviceNumber]->m_pUserRawPtr = (uint16_t*)userLabels.getPixels();
			for(int y=0;y<m_Devices[iDeviceNumber]->m_iDepthImgHeight;y++)
				for(int x=0;x<m_Devices[iDeviceNumber]->m_iDepthImgWidth;x++)
					if(m_Devices[iDeviceNumber]->m_pUserRawPtr[y*m_Devices[iDeviceNumber]->m_iDepthImgWidth + x] >0)
						m_Devices[iDeviceNumber]->m_pUserRawPtr[y*m_Devices[iDeviceNumber]->m_iDepthImgWidth + x] = UINT16_MAX;

			m_Devices[iDeviceNumber]->m_UserSurface = Surface(Channel16u(m_Devices[iDeviceNumber]->m_iDepthImgWidth, m_Devices[iDeviceNumber]->m_iDepthImgHeight, m_Devices[iDeviceNumber]->m_DepthFrame.getStrideInBytes(), 1, m_Devices[iDeviceNumber]->m_pUserRawPtr));
			
			const nite::Array<nite::UserData>& users = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers();
			m_Devices[iDeviceNumber]->m_UserCount = users.getSize();
			for (int i = 0; i < m_Devices[iDeviceNumber]->m_UserCount; ++i)
			{
				const nite::UserData& user = users[i];

				printUserState(iDeviceNumber, user, m_Devices[iDeviceNumber]->m_UserTrackerFrame.getTimestamp());
				if (user.isNew())
				{
					m_Devices[iDeviceNumber]->m_pUserTracker->startSkeletonTracking(user.getId());
					m_Devices[iDeviceNumber]->m_pUserTracker->startPoseDetection(user.getId(), nite::POSE_CROSSED_HANDS);
				}
			}
		}
	}
	
	// read streams image data
	// rgb stream
	if(m_Devices[iDeviceNumber]->m_bRGBStreamActive && m_Devices[iDeviceNumber]->m_RGBStream.isValid()) 
	{
		if(m_Devices[iDeviceNumber]->m_RGBStream.readFrame(&m_Devices[iDeviceNumber]->m_RGBFrame) ==openni::STATUS_OK)
			m_Devices[iDeviceNumber]->m_pRgbRawPtr = (uint8_t*)m_Devices[iDeviceNumber]->m_RGBFrame.getData();
	}
	// depth stream		
	if(m_Devices[iDeviceNumber]->m_bDepthStreamActive && m_Devices[iDeviceNumber]->m_DepthStream.isValid())
	{
		if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		{
			if(m_Devices[iDeviceNumber]->m_DepthStream.readFrame(&m_Devices[iDeviceNumber]->m_DepthFrame)== openni::STATUS_OK)		// just get depth frame if not already polled before otherwise user and depth image aren't synced
				m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr = (uint16_t*) m_Devices[iDeviceNumber]->m_DepthFrame.getData();
		}
		else
			m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr = (uint16_t*) m_Devices[iDeviceNumber]->m_DepthFrame.getData();	// just get the data and not read the frame anew as this has been done already in the user read frame above
	}
	// ir stream
	if(m_Devices[iDeviceNumber]->m_bIRStreamActive && m_Devices[iDeviceNumber]->m_IRStream.isValid())
	{
		if(m_Devices[iDeviceNumber]->m_IRStream.readFrame(&m_Devices[iDeviceNumber]->m_IRFrame) == openni::STATUS_OK)
			m_Devices[iDeviceNumber]->m_pIrRawPtr = (uint8_t*)m_Devices[iDeviceNumber]->m_IRFrame.getData();
	}

	// subtract background if needed
	if(m_Devices[iDeviceNumber]->m_bUserStreamActive && m_Devices[iDeviceNumber]->m_pUserTracker->isValid())
		if(m_Devices[iDeviceNumber]->m_bSubtractBackground && isOneUserVisible(iDeviceNumber))
		{
			for(int y=0;y<m_Devices[iDeviceNumber]->m_DepthFrame.getHeight();y++)
				for(int x=0;x<m_Devices[iDeviceNumber]->m_DepthFrame.getWidth();x++)
					if(m_Devices[iDeviceNumber]->m_pUserRawPtr[y*m_Devices[iDeviceNumber]->m_DepthFrame.getWidth() + x] == 0)
					{
						if(m_Devices[iDeviceNumber]->m_pRgbRawPtr!=nullptr)
						{
							m_Devices[iDeviceNumber]->m_pRgbRawPtr[y*m_Devices[iDeviceNumber]->m_RGBFrame.getWidth()*3 + x*3] = 0;
							m_Devices[iDeviceNumber]->m_pRgbRawPtr[y*m_Devices[iDeviceNumber]->m_RGBFrame.getWidth()*3 + x*3 + 1] = 0;
							m_Devices[iDeviceNumber]->m_pRgbRawPtr[y*m_Devices[iDeviceNumber]->m_RGBFrame.getWidth()*3 + x*3 + 2] = 0;
						}
						if(m_Devices[iDeviceNumber]->m_pIrRawPtr!=nullptr)
							m_Devices[iDeviceNumber]->m_pIrRawPtr[y*m_Devices[iDeviceNumber]->m_IRFrame.getWidth() + x] = 0;
						if(m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr!=nullptr)
							m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr[y*m_Devices[iDeviceNumber]->m_DepthFrame.getWidth() + x] = 0;
					}
		}

	// convert to cinder
	if(m_Devices[iDeviceNumber]->m_bRGBStreamActive && m_Devices[iDeviceNumber]->m_RGBStream.isValid() && m_Devices[iDeviceNumber]->m_pRgbRawPtr!=nullptr) 
	{
		m_Devices[iDeviceNumber]->m_RGBSurface = Surface8u(m_Devices[iDeviceNumber]->m_pRgbRawPtr,  m_Devices[iDeviceNumber]->m_iRgbImgWidth, m_Devices[iDeviceNumber]->m_iRgbImgHeight, m_Devices[iDeviceNumber]->m_RGBFrame.getStrideInBytes(), SurfaceChannelOrder::RGB);
	}
	if(m_Devices[iDeviceNumber]->m_bDepthStreamActive && m_Devices[iDeviceNumber]->m_DepthStream.isValid() && m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr!=nullptr)
	{
		m_Devices[iDeviceNumber]->m_Depth16BitSurface = Surface16u(Channel16u( m_Devices[iDeviceNumber]->m_iDepthImgWidth, m_Devices[iDeviceNumber]->m_iDepthImgHeight, m_Devices[iDeviceNumber]->m_DepthFrame.getStrideInBytes(), 1, m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr));
		
		// normalizing values for 8bit depth display, this is done every frame, so not very performant
		uint32_t size=m_Devices[iDeviceNumber]->m_iDepthImgWidth * m_Devices[iDeviceNumber]->m_iDepthImgHeight;
		if(m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr == nullptr)
			m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr =  new unsigned char[size];
		int normalizing = 10000; // 10meters max 
		for( unsigned int i=0; i<size; ++i )
		{
			m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr[i] = (unsigned char) ( m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr[i] * ( (float)( 255 ) / normalizing ) ); 	
		}
		m_Devices[iDeviceNumber]->m_Depth8BitSurface = Surface(Channel(m_Devices[iDeviceNumber]->m_iDepthImgWidth, m_Devices[iDeviceNumber]->m_iDepthImgHeight, m_Devices[iDeviceNumber]->m_iDepthImgWidth
			, sizeof(char), m_Devices[iDeviceNumber]->m_pDepth8BitRawPtr)).clone();
	}
	if(m_Devices[iDeviceNumber]->m_bIRStreamActive && m_Devices[iDeviceNumber]->m_IRStream.isValid() && m_Devices[iDeviceNumber]->m_pIrRawPtr!=nullptr)
	{
		m_Devices[iDeviceNumber]->m_IRSurface = Surface8u(Channel8u(m_Devices[iDeviceNumber]->m_iIRImgWidth, m_Devices[iDeviceNumber]->m_iIRImgHeight, m_Devices[iDeviceNumber]->m_IRFrame.getStrideInBytes(), 1, m_Devices[iDeviceNumber]->m_pIrRawPtr));
	}
	
}

#define USER_MESSAGE(msg) {	sprintf_s(m_Devices[iDeviceNumber]->m_cUserStatusLabels[user.getId()],"%s", msg);	printf("OpenNI:[%08llu] User #%d:\t%s\n",ts, user.getId(),msg);}
void OpenNI2xWrapper::printUserState(uint16_t iDeviceNumber, const nite::UserData& user, uint64_t ts)
{
	if(iDeviceNumber>=m_Devices.size())
		return;

	if (user.isNew())
	{
		USER_MESSAGE("New");
	}
	else if (user.isVisible() && !m_Devices[iDeviceNumber]->m_bVisibleUsers[user.getId()])
		printf("[%08llu] User #%d:\tVisible\n", ts, user.getId());
	else if (!user.isVisible() && m_Devices[iDeviceNumber]->m_bVisibleUsers[user.getId()])
		printf("[%08llu] User #%d:\tOut of Scene\n", ts, user.getId());
	else if (user.isLost())
	{
		USER_MESSAGE("Lost");
	}
	m_Devices[iDeviceNumber]->m_bVisibleUsers[user.getId()] = user.isVisible();

	if(m_Devices[iDeviceNumber]->m_SkeletonStates[user.getId()] != user.getSkeleton().getState())
	{
		switch(m_Devices[iDeviceNumber]->m_SkeletonStates[user.getId()] = user.getSkeleton().getState())
		{
		case nite::SKELETON_NONE:
			USER_MESSAGE("Stopped tracking.")
			break;
		case nite::SKELETON_CALIBRATING:
			USER_MESSAGE("Calibrating...")
			break;
		case nite::SKELETON_TRACKED:
			USER_MESSAGE("Tracking!")
			break;
		case nite::SKELETON_CALIBRATION_ERROR_NOT_IN_POSE:
		case nite::SKELETON_CALIBRATION_ERROR_HANDS:
		case nite::SKELETON_CALIBRATION_ERROR_LEGS:
		case nite::SKELETON_CALIBRATION_ERROR_HEAD:
		case nite::SKELETON_CALIBRATION_ERROR_TORSO:
			USER_MESSAGE("Calibration Failed... :-|")
			break;
		}
	}
}

bool OpenNI2xWrapper::startRecording(uint16_t iDeviceNumber, std::string fileName, bool isLossyCompressed)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	if(iDeviceNumber < m_Devices.size() && m_Recorder.create(fileName.c_str()) == openni::STATUS_OK)
	{
		if(m_Devices[iDeviceNumber]->m_bRGBStreamActive)
			if(m_Recorder.attach(m_Devices[iDeviceNumber]->m_RGBStream, isLossyCompressed) != openni::STATUS_OK)
				std::cout << "OpenNI: Couldn't attach RGB stream" << std::endl;
		if(m_Devices[iDeviceNumber]->m_bIRStreamActive)
			if(m_Recorder.attach(m_Devices[iDeviceNumber]->m_IRStream, isLossyCompressed) != openni::STATUS_OK)
				std::cout << "OpenNI: Couldn't attach IR stream" << std::endl;
		if(m_Devices[iDeviceNumber]->m_bDepthStreamActive || m_Devices[iDeviceNumber]->m_bUserStreamActive)
			if(m_Recorder.attach(m_Devices[iDeviceNumber]->m_DepthStream, isLossyCompressed) != openni::STATUS_OK)
				std::cout << "OpenNI: Couldn't attach Depth stream" << std::endl;;

		std::cout << "OpenNI: Start recording" << std::endl;
		m_Recorder.start();
		return true;
	}
	else
	{
		std::cout << "OpenNI: Failed to start recording" << std::endl;
		return false;
	}
}

void OpenNI2xWrapper::stopRecording()
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	std::cout << "OpenNI: Stop recording" << std::endl;
	m_Recorder.stop();
	m_Recorder.destroy();
}

int16_t OpenNI2xWrapper::startPlayback(std::string fileName)		// return device index
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	std::shared_ptr<OpenNIDevice> recorderDevice(new OpenNIDevice());

	if(recorderDevice->m_Device.open((fileName).c_str()) != openni::STATUS_OK)
	{
		std::cout << "OpenNI: Couldn't find file " << fileName << std::endl;
		return false;
	}
		
	recorderDevice->m_Player = recorderDevice->m_Device.getPlaybackControl();
	if(!recorderDevice->m_Player->isValid())
		-1;
	recorderDevice->m_Player->setRepeatEnabled(true);
	
	recorderDevice->m_bRGBStreamActive = recorderDevice->m_Device.hasSensor(openni::SENSOR_COLOR);
	recorderDevice->m_bDepthStreamActive = recorderDevice->m_Device.hasSensor(openni::SENSOR_DEPTH);
	recorderDevice->m_bUserStreamActive = recorderDevice->m_bDepthStreamActive;
	recorderDevice->m_bIRStreamActive = recorderDevice->m_Device.hasSensor(openni::SENSOR_IR);

	m_Devices.push_back(recorderDevice);	// push in list of all running devices

	std::cout << "OpenNI: Start playback " << fileName << std::endl;
	startStreams(m_Devices.size()-1, recorderDevice->m_bRGBStreamActive, recorderDevice->m_bDepthStreamActive, recorderDevice->m_bUserStreamActive, recorderDevice->m_bIRStreamActive);
	
	return m_Devices.size()-1;
}

void OpenNI2xWrapper::stopPlayback(uint16_t iRecordId)
{
	if(iRecordId>=m_Devices.size())
		return;

	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	stopDevice(iRecordId);
	m_Devices.erase(m_Devices.begin()+iRecordId);
}

uint16_t OpenNI2xWrapper::getUserCount(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	if(m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers().getSize();
	return 0;
}

void OpenNI2xWrapper::setDepthColorImageAlignment(uint16_t iDeviceNumber,  bool bEnabled)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}

	if(bEnabled)
		m_Devices[iDeviceNumber]->m_Device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	else
		m_Devices[iDeviceNumber]->m_Device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_OFF);
}

void OpenNI2xWrapper::setDepthColorSync(uint16_t iDeviceNumber, bool bEnabled)
{
	// freezes app this is a bug in openni
	m_Devices[iDeviceNumber]->m_Device.setDepthColorSyncEnabled(bEnabled);
}

void OpenNI2xWrapper::setAllStreamsMirrored(uint16_t iDeviceNumber, bool bEnabled)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}

	if(m_Devices[iDeviceNumber]->m_RGBStream.setMirroringEnabled(bEnabled)!=openni::STATUS_OK)
		printf("OpenNI: Couldn't mirror RGB Stream\n");


	if(m_Devices[iDeviceNumber]->m_DepthStream.setMirroringEnabled(bEnabled)!=openni::STATUS_OK)
		printf("OpenNI: Couldn't mirror Depth Stream\n");
	if(m_Devices[iDeviceNumber]->m_IRStream.setMirroringEnabled(bEnabled)!=openni::STATUS_OK)
		printf("OpenNI: Couldn't mirror IR Stream\n");
}

void OpenNI2xWrapper::setBackgroundSubtraction(uint16_t iDeviceNumber, bool bEnabled)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}
	m_Devices[iDeviceNumber]->m_bSubtractBackground = bEnabled;
}

uint16_t OpenNI2xWrapper::getRgbWidth(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iRgbImgWidth;
}

uint16_t OpenNI2xWrapper::getRgbHeight(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iRgbImgHeight;
}

uint16_t OpenNI2xWrapper::getIrWidth(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iIRImgWidth;
}

uint16_t OpenNI2xWrapper::getIrHeight(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iIRImgHeight;
}

uint16_t OpenNI2xWrapper::getDepthWidth(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iDepthImgWidth;
}

uint16_t OpenNI2xWrapper::getDepthHeight(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iDepthImgHeight;
}

uint16_t OpenNI2xWrapper::getUserWidth(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iUserImgWidth;
}

uint16_t OpenNI2xWrapper::getUserHeight(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return 0;
	}
	return m_Devices[iDeviceNumber]->m_iUserImgHeight;
}

Surface16u OpenNI2xWrapper::getDepth16BitSurface(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Surface16u();
	}

	if(m_Devices[iDeviceNumber]->m_Depth16BitSurface)
		return m_Devices[iDeviceNumber]->m_Depth16BitSurface;
	else
		return Surface16u();
}

gl::Texture OpenNI2xWrapper::getDepth16BitTexture(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return cinder::gl::Texture();
	}

	if(m_Devices[iDeviceNumber]->m_Depth16BitSurface)
		m_Devices[iDeviceNumber]->m_Depth16BitTexture = gl::Texture(m_Devices[iDeviceNumber]->m_Depth16BitSurface);
	
	return m_Devices[iDeviceNumber]->m_Depth16BitTexture;
}

Surface OpenNI2xWrapper::getDepth8BitSurface(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Surface();
	}

	if(m_Devices[iDeviceNumber]->m_Depth8BitSurface)
		return Surface(m_Devices[iDeviceNumber]->m_Depth8BitSurface);
	else
		return Surface();
}

gl::Texture OpenNI2xWrapper::getDepth8BitTexture(uint16_t iDeviceNumber)
{	
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return cinder::gl::Texture();
	}

	if(m_Devices[iDeviceNumber]->m_Depth8BitSurface)
		m_Devices[iDeviceNumber]->m_Depth8BitTexture = gl::Texture(m_Devices[iDeviceNumber]->m_Depth8BitSurface);
	
	return m_Devices[iDeviceNumber]->m_Depth8BitTexture;
}

Surface OpenNI2xWrapper::getRGBSurface(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Surface();
	}

	if(m_Devices[iDeviceNumber]->m_RGBSurface)
		return m_Devices[iDeviceNumber]->m_RGBSurface;
	else
		return Surface();
}

gl::Texture OpenNI2xWrapper::getRGBTexture(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return cinder::gl::Texture();
	}

	if(m_Devices[iDeviceNumber]->m_RGBSurface)
		m_Devices[iDeviceNumber]->m_RGBTexture = gl::Texture(m_Devices[iDeviceNumber]->m_RGBSurface);

	return m_Devices[iDeviceNumber]->m_RGBTexture;
}

Surface OpenNI2xWrapper::getIRSurface(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Surface();
	}

	if(m_Devices[iDeviceNumber]->m_IRSurface)
		return m_Devices[iDeviceNumber]->m_IRSurface;
	else
		return Surface();
}

gl::Texture OpenNI2xWrapper::getIRTexture(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return cinder::gl::Texture();
	}

	if(m_Devices[iDeviceNumber]->m_IRSurface)
		m_Devices[iDeviceNumber]->m_IRTexture = gl::Texture(m_Devices[iDeviceNumber]->m_IRSurface);

	return m_Devices[iDeviceNumber]->m_IRTexture;
}

Surface OpenNI2xWrapper::getUserSurface(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Surface();
	}

	if(m_Devices[iDeviceNumber]->m_UserSurface)
		return m_Devices[iDeviceNumber]->m_UserSurface;
	else
		return Surface();
}

gl::Texture OpenNI2xWrapper::getUserTexture(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return cinder::gl::Texture();
	}

	if(m_Devices[iDeviceNumber]->m_UserSurface)
		m_Devices[iDeviceNumber]->m_UserTexture = gl::Texture(m_Devices[iDeviceNumber]->m_UserSurface);
	
	return m_Devices[iDeviceNumber]->m_UserTexture;
}

uint16_t* OpenNI2xWrapper::getDepth16BitImgPtr(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return nullptr;
	}
	return 	m_Devices[iDeviceNumber]->m_pDepth16BitRawPtr;
}

uint8_t* OpenNI2xWrapper::getDepth8BitImgPtr(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return nullptr;
	}
	return 	m_Devices[iDeviceNumber]->m_pRgbRawPtr;
}

uint8_t* OpenNI2xWrapper::getRgbImgPtr(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return nullptr;
	}
	return 	m_Devices[iDeviceNumber]->m_pRgbRawPtr;
}

uint8_t* OpenNI2xWrapper::getIrImgPtr(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return nullptr;
	}
	return m_Devices[iDeviceNumber]->m_pIrRawPtr;
}

uint16_t* OpenNI2xWrapper::getUserImgPtr(uint16_t iDeviceNumber)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return nullptr;
	}
	return m_Devices[iDeviceNumber]->m_pUserRawPtr;
}

bool OpenNI2xWrapper::isOneUserVisible(uint16_t iDeviceNumber)
{
	for(int i=0; i < getUserCount(iDeviceNumber); i++)
		if(isUserVisible(iDeviceNumber, i))
			return true;
	return false;
}


bool OpenNI2xWrapper::isUserVisible(uint16_t iDeviceNumber, uint16_t iUserID)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return false;
	}

	if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return false;

	const nite::Array<nite::UserData>& users = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers();
	if(iUserID < users.getSize())
		return users[iUserID].isVisible();

	return false;
}

Vec3f OpenNI2xWrapper::getUserCenterOfMass(uint16_t iDeviceNumber, uint16_t iUserID)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Vec3f();
	}

	if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return Vec3f();

	float coordinates[3] = {0};

	const nite::Array<nite::UserData>& users = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers();
	const nite::UserData& user = users[iUserID];

	m_Devices[iDeviceNumber]->m_pUserTracker->convertJointCoordinatesToDepth(user.getCenterOfMass().x, user.getCenterOfMass().y, user.getCenterOfMass().z, &coordinates[0], &coordinates[1]);
	// normalize
	coordinates[0] = coordinates[0] / (float)m_Devices[iDeviceNumber]->m_DepthFrame.getWidth();
	coordinates[1] = coordinates[1] / (float)m_Devices[iDeviceNumber]->m_DepthFrame.getHeight();

	return Vec3f(coordinates[0],coordinates[1],user.getCenterOfMass().z);
}

std::vector<OpenNIJoint> OpenNI2xWrapper::getUserSkeletonJoints(uint16_t iDeviceNumber, uint16_t iUserID)			
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return std::vector<OpenNIJoint>();
	}
	if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return std::vector<OpenNIJoint>();

	const nite::Array<nite::UserData>& users = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers();
	const nite::UserData& user = users[iUserID];
	
	std::vector<OpenNIJoint> joints;

	for(int i=0; i<15; i++)
	{
		nite::SkeletonJoint joint =  user.getSkeleton().getJoint((nite::JointType)i);
		OpenNIJoint tmp;
		Vec3f pos;
		m_Devices[iDeviceNumber]->m_pUserTracker->convertJointCoordinatesToDepth(joint.getPosition().x, joint.getPosition().y, joint.getPosition().z, &pos.x, &pos.y);
		pos.x = pos.x / (float)m_Devices[iDeviceNumber]->m_DepthFrame.getWidth();
		pos.y = pos.y / (float)m_Devices[iDeviceNumber]->m_DepthFrame.getHeight();
		pos.z=0;
		tmp.m_Position = pos;
		tmp.m_Orientation = ci::Quatf(joint.getOrientation().x, joint.getOrientation().y, joint.getOrientation().z, joint.getOrientation().w);
		tmp.m_PositionConfidence = joint.getPositionConfidence();
		tmp.m_OrientationConfidence = joint.getOrientationConfidence();
		joints.push_back(tmp);
	}
	return joints;
}

ci::Rectf OpenNI2xWrapper::getUserBoundingBox(uint16_t iDeviceNumber, uint16_t iUserID)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return Rectf();
	}
	if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return Rectf();

	const nite::Array<nite::UserData>& users = m_Devices[iDeviceNumber]->m_UserTrackerFrame.getUsers();
	const nite::UserData& user = users[iUserID];
	float w=(float)m_Devices[iDeviceNumber]->m_DepthFrame.getWidth();
	float h=(float)m_Devices[iDeviceNumber]->m_DepthFrame.getHeight();
	return ci::Rectf(ci::Vec2f(user.getBoundingBox().min.x / w, user.getBoundingBox().min.y / h), 
		ci::Vec2f(user.getBoundingBox().max.x / w, user.getBoundingBox().max.y / h));
}

void OpenNI2xWrapper::drawSkeletons(uint16_t iDeviceNumber, ci::Rectf rect)
{
	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}
	if(!m_Devices[iDeviceNumber]->m_bUserStreamActive)
		return;

	float fRadius = 4.0;
	gl::pushMatrices();
	glLineWidth(2.0);
		
	for(uint16_t i = 0; i < getUserCount(iDeviceNumber); ++i)
	{	
		std::vector<OpenNIJoint> joints = getUserSkeletonJoints(iDeviceNumber, i);		
				
		for(uint16_t j = 0; j< joints.size(); j++)
		{
			if(joints[j].m_OrientationConfidence>0)
			{
				fRadius = 8.0;
				gl::pushModelView();
				gl::translate(joints[j].m_Position.x * rect.getWidth() + rect.getX1(), joints[j].m_Position.y * rect.getHeight() + rect.getY1(), 0);
				gl::rotate(joints[j].m_Orientation);
				gl::drawCoordinateFrame(fRadius);
				gl::popModelView();
			}
			else
			{
				fRadius = 4.0;
				glColor3f( 0, 1.0, 0.0 );		
				gl::drawSolidCircle( Vec2f( joints[j].m_Position.x * rect.getWidth() + rect.getX1(), joints[j].m_Position.y * rect.getHeight() + rect.getY1()), fRadius);
			}
		}
	}	
	glColor3f( 1.0, 1.0, 1.0 );	
	glLineWidth(1.0);
	gl::popMatrices();
}

void OpenNI2xWrapper::debugDraw(uint16_t iDeviceNumber)
{
	std::lock_guard<std::recursive_mutex> lock(m_Mutex);	

	if(iDeviceNumber>=m_Devices.size())
	{
		cout << "OpenNI: Device " << iDeviceNumber << " not initialized or running" << endl;
		return;
	}

	int windowPos=0;
	gl::pushMatrices();
	gl::setViewport( getWindowBounds() );
	gl::setMatricesWindow( getWindowSize() );

	if(m_Devices[iDeviceNumber]->m_bRGBStreamActive)
		m_Devices[iDeviceNumber]->m_RGBTexture = getRGBTexture(iDeviceNumber);
	if(m_Devices[iDeviceNumber]->m_bIRStreamActive)
		m_Devices[iDeviceNumber]->m_IRTexture = getIRTexture(iDeviceNumber);
	if(m_Devices[iDeviceNumber]->m_bDepthStreamActive)
	{
		m_Devices[iDeviceNumber]->m_Depth16BitTexture = getDepth16BitTexture(iDeviceNumber);
		m_Devices[iDeviceNumber]->m_Depth8BitTexture = getDepth8BitTexture(iDeviceNumber);
	}
	if(m_Devices[iDeviceNumber]->m_bUserStreamActive)
		m_Devices[iDeviceNumber]->m_UserTexture = getUserTexture(iDeviceNumber);

	int numberOfDevices = 4;

	float x1 = (float)iDeviceNumber * (float)getWindowWidth()/numberOfDevices;
	float x2 = (float)(iDeviceNumber+1) * (float)getWindowWidth()/numberOfDevices;


	if(m_Devices[iDeviceNumber]->m_RGBTexture)
	{
		gl::draw(m_Devices[iDeviceNumber]->m_RGBTexture, Rectf(x1,0.0f,x2,(float)getWindowHeight()/4.0f) );
		windowPos++;
	}
	if(m_Devices[iDeviceNumber]->m_IRTexture)
	{
		gl::draw(m_Devices[iDeviceNumber]->m_IRTexture, Rectf(x1,(float)windowPos *(float)getWindowHeight()/4.0f, x2, (float)(windowPos+1) * (float)getWindowHeight()/4.0f));
		windowPos++;
	}
	if(m_Devices[iDeviceNumber]->m_Depth8BitTexture)
	{
		gl::draw(m_Devices[iDeviceNumber]->m_Depth8BitTexture,  Rectf(x1, (float)windowPos *(float)getWindowHeight()/4.0f, x2,  (float)(windowPos+1) * (float)getWindowHeight()/4.0f) );
		windowPos++;
	}
	if(m_Devices[iDeviceNumber]->m_UserTexture)
	{
		gl::draw(m_Devices[iDeviceNumber]->m_UserTexture,  Rectf(x1, (float)windowPos *(float)getWindowHeight()/4.0f, x2, (float)(windowPos+1) * (float)getWindowHeight()/4.0f) );
		// draw centroids & bounding boxes
		for(int i=0; i<getUserCount(iDeviceNumber); i++)
		{
			Vec3f pos = getUserCenterOfMass(iDeviceNumber, i);
			pos.x=pos.x*(float)getWindowWidth()/numberOfDevices + x1;
			pos.y=pos.y*(float)(getWindowHeight()/4.0f) + (float)windowPos*(float)getWindowHeight()/4.0f;
			Rectf bb = getUserBoundingBox(iDeviceNumber, i);
			bb.x1=bb.x1*(float)getWindowWidth()/numberOfDevices + x1;
			bb.x2=bb.x2*(float)getWindowWidth()/numberOfDevices + x1;
			bb.y1=bb.y1*(float)(getWindowHeight()/4.0f) + (float)windowPos*(float)getWindowHeight()/4.0f;
			bb.y2=bb.y2*(float)(getWindowHeight()/4.0f) + (float)windowPos*(float)getWindowHeight()/4.0f;
			gl::color(1,0,0);
			gl::drawSolidCircle(Vec2f(pos.x, pos.y),3);
			gl::drawStrokedRect(bb);
			gl::color(1,1,1);
		}
		windowPos++;
	}

	drawSkeletons(iDeviceNumber, Rectf(x1,(float)(getWindowHeight()/4.0f)*3,x2,(float)(getWindowHeight())));

	gl::popMatrices();
}
