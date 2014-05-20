#ifndef _TEST_APP
#define _TEST_APP

#include "ofxOpenNI.h"
#include "ofMain.h"
#include "ofxUI.h"
#include "ofxOsc.h"

struct KUsersBoundingBox {
    
  ofVec3f min;
  ofVec3f max;
  ofVec3f center;
  ofxOpenNIUser * current;
  bool inside;
  long starttime;
  float size;
  
};

struct KUsersRepere {
    
    ofVec3f pos;
    float radius;
    
};

struct KUsersOscConfig {
    
    string ip;
    int port;
    string address;
    
};

struct KUsersAction {
    
    string name;
    float min;
    float max;
    bool active;
    
};

struct KUsersSettings {

    int mode;
    float calibration_time;
    vector< KUsersOscConfig * > osc_conf;
    vector< KUsersAction * > actions;

};

class kUsersApp : public ofBaseApp{

    public:
    
	void setup();
	void update();
	void draw();
        void exit();
            
	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void windowResized(int w, int h);

        void userEvent(ofxOpenNIUserEvent & event);
        void guiEvent( ofxUIEventArgs &e );
        
    private:
        
        void loadSettings();
        void saveSettings();
        
        bool testUser( ofxOpenNIUser * );
        bool analyseUser( ofxOpenNIUser * );
        
        void sendMessage( string, string );
        void sendMessage( string, int );
        
        int userSeen;
        vector< ofxOpenNIUser * > users;
        KUsersBoundingBox bb;
	ofxOpenNI openNIDevice;
        
        KUsersRepere rep;
        KUsersSettings settings;
        vector< KUsersAction * >::iterator itca;

        ofxOscSender * senders;
        
        ofxUICanvas * gui;
        
        int status;
        
};

#endif
