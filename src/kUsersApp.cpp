#include "kUsersApp.h"
#include "ofxXmlSettings.h"

//--------------------------------------------------------------
void kUsersApp::setup() {
    
    ofSetLogLevel( OF_LOG_ERROR );

    openNIDevice.setup();
    openNIDevice.addImageGenerator();
    openNIDevice.addDepthGenerator();
    openNIDevice.setRegister(true);
    openNIDevice.setMirror(true);
    openNIDevice.addUserGenerator();
    openNIDevice.setMaxNumUsers( 10 );
    openNIDevice.start();
    
//    openNIDevice.setUseMaskPixelsAllUsers(true); // if you just want pixels, use this set to true
//    openNIDevice.setUseMaskTextureAllUsers(true); // this turns on mask pixels internally AND creates mask textures efficiently
    openNIDevice.setUsePointCloudsAllUsers(true);
    openNIDevice.setPointCloudDrawSizeAllUsers( 4 ); // size of each 'point' in the point cloud
    openNIDevice.setPointCloudResolutionAllUsers( 4 ); // resolution of the mesh created for the point cloud eg., this will use every second depth pixel
    
    userSeen = 0;
    status = 0;
    
    rep.pos.y = 1000;
    rep.pos.z = 2200;
    rep.radius = 250;
    
    loadSettings();
    
    senders = new ofxOscSender[ settings.osc_conf.size() ];
    for ( int i = 0; i < settings.osc_conf.size(); i++ ) {
        KUsersOscConfig * oc = settings.osc_conf[ i ];
        senders[ i ].setup( oc->ip, oc->port );
        cout << oc->ip << ":" << oc->port << endl;
    }
    
    sendMessage( "info", "HELLO" );
    
    gui = new ofxUICanvas();
    gui->addLabel( "REPERE", OFX_UI_FONT_SMALL );
    gui->addSlider( "x pos", -2000, 2000, &rep.pos.x, 300, 10 );
    gui->addSlider( "y pos", -2000, 2000, &rep.pos.y, 300, 10 );
    gui->addSlider( "z pos", 0, 4000, &rep.pos.z, 300, 10 );
    gui->addSlider( "radius", 0, 1000, &rep.radius, 300, 10 );
    gui->addLabel( "ACTIONS", OFX_UI_FONT_SMALL );
    gui->addSlider( "calibration (millis)", 0, 60000, &settings.calibration_time, 300, 10 );
    for ( int i = 0; i < settings.actions.size(); i++ ) {
        KUsersAction * ac = settings.actions[ i ];
        gui->addRangeSlider( ac->name, 0.0, 2.0, &ac->min, &ac->max, 300, 10 );
    }
    gui->setColorBack( ofxUIColor( 255,255,255,90 ) );
    ofAddListener( gui->newGUIEvent, this, &kUsersApp::guiEvent );
    
}

void kUsersApp::loadSettings() {
    
    ofxXmlSettings xml;
    if ( !xml.loadFile( "settings.xml" ) ) {
        cerr << "Impossible to load data/settings.xml, application will exit." << endl;
        ofExit();
    }
    xml.pushTag( "kUsers" );
    
    settings.mode = xml.getAttribute( "mode", "type", 1, 0 );
    settings.calibration_time = xml.getAttribute( "calibration", "time", 1, 0 );
    rep.pos.x = xml.getAttribute( "spot", "x", rep.pos.x, 0 );
    rep.pos.y = xml.getAttribute( "spot", "y", rep.pos.y, 0 );
    rep.pos.z = xml.getAttribute( "spot", "z", rep.pos.z, 0 );
    rep.radius = xml.getAttribute( "spot", "radius", rep.radius, 0 );
    
    xml.pushTag( "osc" );
    xml.pushTag( "senders" );
    int count = xml.getNumTags( "sender" );
    for ( int i = 0; i < count; i++ ) {
        KUsersOscConfig * oc = new KUsersOscConfig();
        oc->ip = xml.getAttribute( "sender", "ip", "", i );
        oc->address = xml.getAttribute( "sender", "address", "", i );
        oc->port = xml.getAttribute( "sender", "port", 0, i );
        settings.osc_conf.push_back( oc );
    }
    xml.popTag();
    xml.popTag();
    
    xml.pushTag( "actions" );
    count = xml.getNumTags( "action" );
    for ( int i = 0; i < count; i++ ) {
        KUsersAction * ac = new KUsersAction();
        ac->name = xml.getAttribute( "action", "name", "", i );
        ac->min = xml.getAttribute( "action", "min", 0.0, i );
        ac->max = xml.getAttribute( "action", "max", 1.0, i );
        ac->active = false;
        settings.actions.push_back( ac );
    }
    xml.popTag();
    
}

void kUsersApp::saveSettings() {
    
    
    ofxXmlSettings xml;
    if ( !xml.loadFile( "settings.xml" ) ) {
        cerr << "Impossible to save data/settings.xml!" << endl;
    }
    xml.pushTag( "kUsers" );
    xml.setAttribute( "mode", "type", settings.mode, 0 );
    xml.setAttribute( "calibration", "time", settings.calibration_time, 0 );
    xml.setAttribute( "spot", "x", rep.pos.x, 0 );
    xml.setAttribute( "spot", "y", rep.pos.y, 0 );
    xml.setAttribute( "spot", "z", rep.pos.z, 0 );
    xml.setAttribute( "spot", "radius", rep.radius, 0 );
    xml.pushTag( "actions" );
    for ( int i = 0; i < settings.actions.size(); i++ ) {
        KUsersAction * ac = settings.actions[ i ];
        xml.setAttribute( "action", "name", ac->name, i );
        xml.setAttribute( "action", "min", ac->min, i );
        xml.setAttribute( "action", "max", ac->max, i );
    }
    xml.popTag();
    xml.save( "settings.xml" );
    
}

//--------------------------------------------------------------

bool kUsersApp::testUser( ofxOpenNIUser * user ) {
    
    if ( user->getPointCloud().getVertices().size() == 0 ) {
        // user not tracked
        return false;
    }
    
    return true;
    
}

bool kUsersApp::analyseUser( ofxOpenNIUser * user ) {
    
    if ( !testUser( user ) )
        return false;
    
    ofMesh & mesh = user->getPointCloud();
    vector< ofVec3f > & vertices = mesh.getVertices();
    
    bb.min.set( 0,0,0 );
    bb.max.set( 0,0,0 );
    bb.center.set( 0,0,0 );
    
    vector< ofVec3f >::iterator itv;  
    for ( itv = vertices.begin(); itv != vertices.end(); itv++ ) {
        ofVec3f & v = (*itv);

        bb.center += v;

        if ( itv == vertices.begin() ) {
            bb.min.set( v );
            bb.max.set( v );
            continue;
        }

        if ( v.x < bb.min.x )
            bb.min.x = v.x;
        if ( v.y < bb.min.y )
            bb.min.y = v.y;
        if ( v.z < bb.min.z )
            bb.min.z = v.z;
        if ( v.x > bb.max.x )
            bb.max.x = v.x;
        if ( v.y > bb.max.y )
            bb.max.y = v.y;
        if ( v.z > bb.max.z )
            bb.max.z = v.z;
    }

    bb.center /= vertices.size();

    if ( ofDist( -rep.pos.x, rep.pos.z, bb.center.x, bb.center.z ) < rep.radius ) {
        
        bb.inside = true;
        
        if ( bb.current != user )
            bb.starttime = ofGetElapsedTimeMillis();

        bb.current = user;
        return true;
        
    }
    
    // resetting bounding box
    bb.current = NULL;
    bb.inside = false;
    bb.starttime = 0;
    bb.size = 0;
    
    return false;
    
}

void kUsersApp::update(){
        
//    bb.current = NULL;
//    bb.inside = false;
    
    openNIDevice.update();
    
    
    
    users.clear();
    userSeen = openNIDevice.getNumTrackedUsers();
    
    if ( bb.current != NULL ) {
        // is the user still visible & in the zone?
        if ( !analyseUser( bb.current ) ) {
            
            sendMessage( "info", "USEROUT" );
            status = 0;
            // reseting all actions
            for ( itca = settings.actions.begin(); itca != settings.actions.end(); itca++ ) {
                KUsersAction * ac = (*itca);
                ac->active = false;
            }
            
        } else {
        
            // gaming!!
            if ( bb.starttime + settings.calibration_time > ofGetElapsedTimeMillis() ) {
                
                if ( status != 1 ) {
                    sendMessage( "info", "USERIN" );
                    status = 1;
                } else {
                    sendMessage( "calibration", int( ( ( ofGetElapsedTimeMillis() - bb.starttime * 1.f ) / settings.calibration_time ) * 100.f ) );
                }
                
            } else {
                
                if ( status != 2 ) {
                
                    status = 2;
                    bb.size = abs( rep.pos.y + bb.max.y );
                    sendMessage( "info", "CALIBRATED" );
                    sendMessage( "size", int( bb.size ) );
//                    sendMessage( "rep_y", int( rep.pos.y ) );
//                    sendMessage( "user_min", int( bb.min.y ) );
//                    sendMessage( "user_max", int( bb.max.y ) );
                
                } else {
                    
                    // actions detection
                    
                    float csize = abs( rep.pos.y + bb.max.y );
                    
                    for ( itca = settings.actions.begin(); itca != settings.actions.end(); itca++ ) {
                        
                        KUsersAction * ac = (*itca);
                        bool wasactive = ac->active;
                        
                        if ( csize > ac->min * bb.size && csize < ac->max * bb.size ) {
                            
                            if ( !wasactive )
                                sendMessage( "action", ac->name );
                            
                            ac->active = true;
                            
                        } else {
                                                        
                            ac->active = false;
                            
                        }
                    }
                    
                }
            }
            
        }
    }
    
    for (int i = 0; i < userSeen; i++){
        
        ofxOpenNIUser & user = openNIDevice.getTrackedUser(i);
        
        if ( bb.current == NULL )
            analyseUser( &user );
        
        if ( testUser( &user ) )
            users.push_back( &user );

    }
}

//--------------------------------------------------------------
void kUsersApp::draw(){

    gui->setDimensions( 350, 250 );
    // gui->setPosition( 0, ofGetHeight() - 200 );
    
    ofBackground( 0,0,0 );

    ofSetColor(255, 255, 255);
    
    glEnable( GL_DEPTH_TEST );
    ofPushMatrix();
    
        ofTranslate( ( ofGetWidth() * 0.5 ), ( ofGetHeight() * 0.5 ), 0 );
        ofRotateZ( 180 );
        ofScale( 1,1,-1 );
        
        // DRAWING REPERE
        ofPushMatrix();
            ofTranslate( -rep.pos.x, -rep.pos.y * 1.001, rep.pos.z );
            ofRotateX( -90 );
            ofSetColor( 127,127,127 );
            ofFill();
            ofCircle( 0,0,0, rep.radius * 1.2 );
        ofPopMatrix();
        ofPushMatrix();
            ofTranslate( -rep.pos.x, -rep.pos.y, rep.pos.z );
            ofRotateX( -90 );
            ofSetColor( 255, 255, 255 );
            if ( bb.inside )
                ofFill();
            else
                ofNoFill();
            ofCircle( 0,0,0, rep.radius );
        ofPopMatrix();
                
        // draw line to rep
        ofSetColor( 255, 255, 255 );
        ofLine( 
                -rep.pos.x, -rep.pos.y, rep.pos.z,
                bb.center.x, -rep.pos.y, bb.center.z );
        ofLine( 
                bb.center.x, -rep.pos.y, bb.center.z,
                bb.center.x, bb.min.y, bb.center.z );

        ofPushMatrix();
            ofTranslate( 0, 0, bb.min.z );
            ofSetColor( 255, 0, 0 );
            ofNoFill();
            ofRect( bb.min.x, bb.min.y, bb.max.x - bb.min.x, bb.max.y - bb.min.y );
        ofPopMatrix();

        ofPushMatrix();
            ofTranslate( bb.center.x, bb.center.y, bb.center.z );
            ofSetColor( 255, 0, 0 );
            ofLine( 0, -20, 0, 20 );
            ofLine( -20, 0, 20, 0 );
        ofPopMatrix();

        ofPushMatrix();
            ofTranslate( 0, 0, bb.max.z );
            ofSetColor( 255, 0, 0 );
            ofNoFill();
            ofRect( bb.min.x, bb.min.y, bb.max.x - bb.min.x, bb.max.y - bb.min.y );
        ofPopMatrix();
         
        ofSetColor(255, 255, 255);
        for ( int i = 0; i < users.size(); i++ )
            users[ i ]->drawPointCloud();
        
    ofPopMatrix();
    glDisable( GL_DEPTH_TEST );
    
//    ofSetColor( 255, 0, 255);
//    ofLine( 0, ofGetHeight() * 0.5, ofGetWidth(), ofGetHeight() * 0.5 );
//    ofLine( ofGetWidth() * 0.5, 0, ofGetWidth() * 0.5, ofGetHeight() );
 
    ofSetColor(255, 255, 255);
    string msg = "MILLIS: " + ofToString(ofGetElapsedTimeMillis()) + " FPS: " + ofToString(ofGetFrameRate()) + " Device FPS: " + ofToString(openNIDevice.getFrameRate());
    ofPushMatrix();
    ofTranslate( 0,250,0 );
    
    ofDrawBitmapString( msg, 5, 20 );
    ofDrawBitmapString( "user seen: " + ofToString( userSeen ), 5, 35 );
    ofDrawBitmapString( "time: " + ofToString( bb.starttime ), 5, 50 );
    
    int calpc; 
    switch( status ) {
        case 0:
            ofDrawBitmapString( "STATUS: QUIET", 5, 65 );
            break;
        case 1:
            calpc = int( ( ( ofGetElapsedTimeMillis() - bb.starttime * 1.f ) / settings.calibration_time ) * 100.f );
            ofDrawBitmapString( "STATUS: CALIBRATING " + ofToString( calpc ) + "%", 5, 65 );
            break;
        case 2:
            ofDrawBitmapString( "STATUS: GAME", 5, 65 );
            ofDrawBitmapString( "ref size: " + ofToString( bb.size ), 5, 80 );
            ofDrawBitmapString( "current size: " + ofToString( abs( rep.pos.y + bb.max.y ) ), 5, 95 );
            break;
    }

    ofPopMatrix();
    
}

//--------------------------------------------------------------
void kUsersApp::userEvent(ofxOpenNIUserEvent & event){
    // show user event messages in the console
    ofLogNotice() << getUserStatusAsString( event.userStatus ) << "for user" << event.id << "from device" << event.deviceID;
}

//--------------------------------------------------------------
void kUsersApp::exit(){
    openNIDevice.stop();
    sendMessage( "info", "EXIT" );
}

//--------------------------------------------------------------
void kUsersApp::keyPressed(int key){

}

//--------------------------------------------------------------
void kUsersApp::keyReleased(int key){

}

//--------------------------------------------------------------
void kUsersApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void kUsersApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void kUsersApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void kUsersApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void kUsersApp::windowResized(int w, int h){

}

void kUsersApp::guiEvent( ofxUIEventArgs &e ) {
    
    saveSettings();
    
}

void kUsersApp::sendMessage( string type, string txt ) {
    
    ofxOscMessage msg;
    msg.addStringArg( txt );
    for ( int i = 0; i < settings.osc_conf.size(); i++ ) {
        KUsersOscConfig * oc = settings.osc_conf[ i ];
        msg.setAddress( oc->address+"/"+type );
        senders[ i ].sendMessage( msg );
    }
    
}

void kUsersApp::sendMessage( string type, int v ) {
    
    ofxOscMessage msg;
    msg.addIntArg( v );
    for ( int i = 0; i < settings.osc_conf.size(); i++ ) {
        KUsersOscConfig * oc = settings.osc_conf[ i ];
        msg.setAddress( oc->address+"/"+type );
        senders[ i ].sendMessage( msg );
    }
    
}