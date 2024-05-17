// separate click radius from draw radius
// abstract DraggablePoint into template
// don't move model when dragging points
// only select one point at a time.

#include "ofMain.h"
#include "ofAppGLFWWindow.h"
#include "ofxAssimpModelLoader.h"
#include "ofxGui.h"
#include "ofxDropdown.h"

#include "SharedCode/Mapamok.h"
#include "DraggablePoints.h"
#include "MeshUtils.h"

class ReferencePoints : public DraggablePoints {
public:
    void draw(ofEventArgs& args) {
        ofPushStyle();
        ofSetColor(ofColor::red);
        DraggablePoints::draw(args);
        ofPopStyle();
    }
};

class ofApp : public ofBaseApp {
public:
    
    ofxPanel gui;
    bool bShowGui = true;
    Mapamok mapamok;
    
    const float cornerRatio = .1;
    const int cornerMinimum = 3;
    const float mergeTolerance = .001;
    const float selectionMergeTolerance = .01;
    
    ofParameter<bool> editToggle;
    ofParameter<bool> loadButton;
    ofParameter<bool> saveButton;
    
    ofParameter<int> renderModes; // = {"drawMode", 0};
    unique_ptr<ofxIntDropdown> uidDropdown_intRenderModes;

    ofParameter<int> backgroundBrightness;
    ofParameter<bool> useShader;
    
    ofVboMesh mesh, cornerMesh;
    ofEasyCam cam;
    ReferencePoints referencePoints;
    
    void setup() {
        ofSetWindowTitle("mapamok");
        ofSetVerticalSync(true);
        setupGui();
        if(ofFile::doesFileExist("model.dae")) {
            loadModel("model.dae");
        } else if(ofFile::doesFileExist("model.3ds")) {
            loadModel("model.3ds");
        }
        cam.setDistance(1);
        cam.setNearClip(.1);
        cam.setFarClip(10);
        
        referencePoints.setClickRadius(8);
        referencePoints.enableControlEvents();
//        referencePoints.enableDrawEvent();
    }
    enum {
        RENDER_MODE_FACES = 0,
        RENDER_MODE_OUTLINE,
        RENDER_MODE_WIREFRAME_FULL,
        RENDER_MODE_WIREFRAME_OCCLUDED
    };
    void setupGui() {
//        ofColor
//        cb(64, 192),
//        co(192, 192),
//        coh(128, 192),
//        cf(240, 255),
//        cfh(128, 255),
//        cp(96, 192),
//        cpo(255, 192);
      

        
        gui.setup("Calibration","gui.json");
        gui.add(editToggle.set("editToggle",true));
        gui.add(loadButton.set("loadButton",false));
        gui.add(saveButton.set("saveButton",false));

//        gui->addLabel("Render");
//        vector<string> renderModes;
//        renderModes.push_back("Faces");
//        renderModes.push_back("Depth Edges");
//        renderModes.push_back("Full wireframe");
//        renderModes.push_back("Occluded wireframe");
//        renderMode = gui->addRadio("Render", renderModes, OFX_UI_ORIENTATION_VERTICAL, OFX_UI_FONT_MEDIUM);
//        renderMode->activateToggle(renderModes[0]);
        
        renderModes.setName("renderModes");
        uidDropdown_intRenderModes = make_unique<ofxIntDropdown>(renderModes);
        uidDropdown_intRenderModes->add(0, "Faces");
        uidDropdown_intRenderModes->add(1, "Depth Edges");
        uidDropdown_intRenderModes->add(2, "Full wireframe");
        uidDropdown_intRenderModes->add(3, "Occluded wireframe");
        uidDropdown_intRenderModes->disableMultipleSelection();
        uidDropdown_intRenderModes->enableCollapseOnSelection();
        
//        gui->addSpacer();
        
        
//        gui->addMinimalSlider("Background", 0, 255, &backgroundBrightness);
//        gui->addToggle("Use shader", &useShader);
        gui.add(backgroundBrightness.set("bgBright",127,0,255));
        gui.add(useShader.set("useShader",false));
//        gui->autoSizeToFitWidgets();
    }
//    int getSelection(ofxUIRadio* radio) {
//        vector<ofxUIToggle*> toggles = radio->getToggles();
//        for(int i = 0; i < toggles.size(); i++) {
//            if(toggles[i]->getValue()) {
//                return i;
//            }
//        }
//        return -1;
//    }
    void render() {
//        int renderModeSelection = getSelection(renderMode);
        if(renderModes.get() == 0){ //RENDER_MODE_FACES) {
            //            ofEnableDepthTest();
            ofSetColor(255, 128);
            mesh.drawFaces();
            //            ofDisableDepthTest();
        } else if(renderModes.get() == 1){ //RENDER_MODE_WIREFRAME_FULL) {
            mesh.drawWireframe();
        } else if(renderModes.get() == 2 || renderModes.get() == 3){ //RENDER_MODE_OUTLINE || renderModeSelection == RENDER_MODE_WIREFRAME_OCCLUDED) {
            prepareRender(true, true, false);
            glEnable(GL_POLYGON_OFFSET_FILL);
            float lineWidth = ofGetStyle().lineWidth;
            if(renderModes == 2){ //RENDER_MODE_OUTLINE) {
                glPolygonOffset(-lineWidth, -lineWidth);
            } else if(renderModes == 3){ //RENDER_MODE_WIREFRAME_OCCLUDED) {
                glPolygonOffset(+lineWidth, +lineWidth);
            }
            glColorMask(false, false, false, false);
            mesh.drawFaces();
            glColorMask(true, true, true, true);
            glDisable(GL_POLYGON_OFFSET_FILL);
            mesh.drawWireframe();
            prepareRender(false, false, false);
        }
    }
    void drawEdit() {
        cam.begin();
        ofPushStyle();
        ofSetColor(255, 128);
        mesh.drawFaces();
        ofPopStyle();
        cam.end();
        
        ofMesh cornerMeshImage = cornerMesh;
        // should only update this if necessary
        project(cornerMeshImage, cam, ofGetWindowRect());
        
        // if this is a new mesh, create the points
        // should use a "dirty" flag instead allowing us to reset manually
        if(cornerMesh.getNumVertices() != referencePoints.size()) {
            referencePoints.clear();
            for(int i = 0; i < cornerMeshImage.getNumVertices(); i++) {
                referencePoints.add(ofVec2f(cornerMeshImage.getVertex(i)));
            }
        }
        
        // if the calibration is ready, use the calibration to find the corner positions
        
        // otherwise, update the points
        for(int i = 0; i < referencePoints.size(); i++) {
            DraggablePoint& cur = referencePoints.get(i);
            if(!cur.hit) {
                cur.position = cornerMeshImage.getVertex(i);
            } else {
                ofDrawLine(cur.position, cornerMeshImage.getVertex(i));
            }
        }
        
        // should be a better way to do this
        ofEventArgs args;
        referencePoints.draw(args);
        
        // calculating the 3d mesh
        vector<ofVec2f> imagePoints;
        vector<ofVec3f> objectPoints;
        for(int i = 0; i < referencePoints.size(); i++) {
            DraggablePoint& cur = referencePoints.get(i);
            if(cur.hit) {
                imagePoints.push_back(cur.position);
                objectPoints.push_back(cornerMesh.getVertex(i));
            }
        }
        
        // should only calculate this when the points are updated
        mapamok.update(ofGetWidth(), ofGetHeight(), imagePoints, objectPoints);
    }
    void draw() {
        ofBackground(backgroundBrightness);
        ofSetColor(255);
        
        if(editToggle) {
            drawEdit();
        }
        
        if(mapamok.calibrationReady) {
            mapamok.begin();
            if(editToggle) {
                ofSetColor(255, 128);
            } else {
                ofSetColor(255);
            }
            mesh.draw();
            mapamok.end();
        }
        
        if(bShowGui) gui.draw();
        ofDrawBitmapString(ofToString((int) ofGetFrameRate()), 10, ofGetHeight() - 40);
    }
    void loadModel(string filename) {
        ofxAssimpModelLoader model;
        model.loadModel(filename);
        vector<ofMesh> meshes = getMeshes(model);
        
        // join all the meshes
        mesh = ofVboMesh();
        mesh = joinMeshes(meshes);
        ofVec3f cornerMin, cornerMax;
        getBoundingBox(mesh, cornerMin, cornerMax);
        centerAndNormalize(mesh, cornerMin, cornerMax);
        
        // normalize submeshes before any further processing
        for(int i = 0; i < meshes.size(); i++) {
            centerAndNormalize(meshes[i], cornerMin, cornerMax);
        }
        
        // merge
        // another good metric for finding corners is if there is a single vertex touching
        // the wall of the bounding box, that point is a good control point
        cornerMesh = ofVboMesh();
        for(int i = 0; i < meshes.size(); i++) {
            ofMesh mergedMesh = mergeNearbyVertices(meshes[i], mergeTolerance);
            vector<unsigned int> cornerIndices = getRankedCorners(mergedMesh);
            int n = cornerIndices.size() * cornerRatio;
            n = MIN(MAX(n, cornerMinimum), cornerIndices.size());
            for(int j = 0; j < n; j++) {
                int index = cornerIndices[j];
                const ofVec3f& corner = mergedMesh.getVertices()[index];
                cornerMesh.addVertex(corner);
            }
        }
        cornerMesh = mergeNearbyVertices(cornerMesh, selectionMergeTolerance);
        cornerMesh.setMode(OF_PRIMITIVE_POINTS);
    }
    void dragEvent(ofDragInfo dragInfo) {
        if(dragInfo.files.size() == 1) {
            string filename = dragInfo.files[0];
            loadModel(filename);
        }
    }
    void keyPressed(int key) {
        if(key == 'f') {
            ofToggleFullscreen();
        }
        if(key == '\t') {
//            gui->toggleVisible();
            bShowGui = !bShowGui;
        }
    }
};

int main() {
    ofAppGLFWWindow window;
    ofSetupOpenGL(&window, 1280, 720, OF_WINDOW);
    ofRunApp(new ofApp());
}
