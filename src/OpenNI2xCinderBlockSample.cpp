#include "cinder/app/AppBasic.h"
#include "cinder/params/Params.h"

#include "OpenNI2xWrapper.h"

using namespace ci;
using namespace ci::app;
using namespace std;

// We'll create a new Cinder Application by deriving from the AppBasic class
class OpenNI2xCinderBlockSample : public AppBasic {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void draw();
	void shutdown();

  private:
	void setMirrored();
	void resetStreams();
	void alignDepthRGB();
	void subtractBg();
	void record();
	void play();
	void stop();

	OpenNI2xWrapper			m_OpenNI2xBlock;
	params::InterfaceGlRef	m_Params;
	bool					m_bMirrored;
	bool					m_bAlign;
	bool					m_bSubtractBg;
	bool					m_bIsRecording;
	std::vector<int>		m_vPlaybackIDs;	
	int						m_iRecording;
			
};

void OpenNI2xCinderBlockSample::prepareSettings( Settings *settings )
{
    settings->enableConsoleWindow();
	settings->setWindowSize(Vec2i(1024,768));
}

void OpenNI2xCinderBlockSample::setup()
{
	m_OpenNI2xBlock.init();
	for(int i=0; i<m_OpenNI2xBlock.getNumberOfConnectedDevices(); i++)
		m_OpenNI2xBlock.startDevice(i);
	
	m_bMirrored=true;		//this is default for openni
	m_bAlign=false;
	m_bSubtractBg=false;
	m_bIsRecording=false;
	m_iRecording=0;

	// Setup the parameters
	m_Params = params::InterfaceGl::create( getWindow(), "OpenNI parameters", toPixels( Vec2i( 200, 400 ) ) );
	m_Params->addButton( "Mirrored", std::bind( &OpenNI2xCinderBlockSample::setMirrored, this ) );
	m_Params->addButton( "Reset", std::bind( &OpenNI2xCinderBlockSample::resetStreams, this ) );
	m_Params->addButton( "Align Depth/RGB", std::bind( &OpenNI2xCinderBlockSample::alignDepthRGB, this ) );
	m_Params->addButton( "Subtract Background", std::bind( &OpenNI2xCinderBlockSample::subtractBg, this ) );
	m_Params->addButton( "Start/Stop Record", std::bind( &OpenNI2xCinderBlockSample::record, this ) );
	m_Params->addButton( "Play", std::bind( &OpenNI2xCinderBlockSample::play, this ) );
	m_Params->addButton( "Stop", std::bind( &OpenNI2xCinderBlockSample::stop, this ) );
}

void OpenNI2xCinderBlockSample::shutdown()
{
	m_OpenNI2xBlock.shutdown();
}

void OpenNI2xCinderBlockSample::keyDown( KeyEvent event )
{
	if( event.getChar() == 'f' )
		setFullScreen( ! isFullScreen() );
}

void OpenNI2xCinderBlockSample::draw()
{
	gl::clear();

	for(int i=0; i<m_OpenNI2xBlock.getNumberOfRunningDevices(); i++)
	{
		m_OpenNI2xBlock.updateDevice(i);
		m_OpenNI2xBlock.debugDraw(i);
	}
	
	// Draw the interface
	m_Params->draw();
	
}

void OpenNI2xCinderBlockSample::setMirrored()
{
	m_bMirrored=!m_bMirrored;
	for(int i=0; i<m_OpenNI2xBlock.getNumberOfRunningDevices(); i++)
		m_OpenNI2xBlock.setAllStreamsMirrored(i, m_bMirrored);

}

void OpenNI2xCinderBlockSample::resetStreams()
{
	for(int i=0; i<m_OpenNI2xBlock.getNumberOfRunningDevices(); i++)
		m_OpenNI2xBlock.resetDevice(i);
}

void OpenNI2xCinderBlockSample::alignDepthRGB()
{
	m_bAlign=!m_bAlign;
	for(int i=0; i<m_OpenNI2xBlock.getNumberOfRunningDevices(); i++)
		m_OpenNI2xBlock.setDepthColorImageAlignment(i, m_bAlign);
}

void OpenNI2xCinderBlockSample::subtractBg()
{
	m_bSubtractBg=!m_bSubtractBg;
	for(int i=0; i<m_OpenNI2xBlock.getNumberOfRunningDevices(); i++)
		m_OpenNI2xBlock.setBackgroundSubtraction(i, m_bSubtractBg);
}

void OpenNI2xCinderBlockSample::record()
{
	if(	!m_bIsRecording )
	{
		std::stringstream fileName;
		fileName << "capture" << m_iRecording << ".oni";
		m_OpenNI2xBlock.startRecording(0, fileName.str(), true);
		m_iRecording++;
	}
	else
		m_OpenNI2xBlock.stopRecording();

	m_bIsRecording=!m_bIsRecording;

}

 void OpenNI2xCinderBlockSample::play()
 {
	 if(m_iRecording>0)
	 {
		std::stringstream fileName;
		fileName << "capture" << m_iRecording-1 << ".oni";
		m_vPlaybackIDs.push_back(m_OpenNI2xBlock.startPlayback(fileName.str()));
	 }
 }

 void OpenNI2xCinderBlockSample::stop()
 {
	 if(m_vPlaybackIDs.size()>0)
	 {
		 m_OpenNI2xBlock.stopPlayback(m_vPlaybackIDs.back());
		 m_vPlaybackIDs.pop_back();
	 }
 }

// This line tells Cinder to actually create the application
CINDER_APP_BASIC( OpenNI2xCinderBlockSample, RendererGl )