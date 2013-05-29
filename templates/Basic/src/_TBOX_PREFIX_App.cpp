#include "cinder/app/AppBasic.h"
#include "cinder/params/Params.h"

#include "OpenNI2xWrapper.h"

using namespace ci;
using namespace ci::app;
using namespace std;


class _TBOX_PREFIX_App : public AppBasic {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void draw();
	void shutdown();

  private:
	OpenNI2xWrapper			m_OpenNI2xBlock;		
};

void _TBOX_PREFIX_App::prepareSettings( Settings *settings )
{
    settings->enableConsoleWindow();
	settings->setWindowSize(Vec2i(1024,768));
}

void _TBOX_PREFIX_App::setup()
{
	// init openni and nite and start up all connected and found devices
	m_OpenNI2xBlock.init();
	for(int i=0; i<m_OpenNI2xBlock.getDevicesConnected(); i++)
		m_OpenNI2xBlock.startDevice(i);
}

void _TBOX_PREFIX_App::shutdown()
{
	m_OpenNI2xBlock.shutdown();
}

void _TBOX_PREFIX_App::keyDown( KeyEvent event )
{
	if( event.getChar() == 'f' )
		setFullScreen( ! isFullScreen() );
}

void _TBOX_PREFIX_App::draw()
{
	gl::clear();

	for(int i=0; i<m_OpenNI2xBlock.getDevicesRunning(); i++)
	{
		m_OpenNI2xBlock.updateDevice(i);
		m_OpenNI2xBlock.debugDraw(i);
	}
}

// This line tells Cinder to actually create the application
CINDER_APP_BASIC( _TBOX_PREFIX_App, RendererGl )