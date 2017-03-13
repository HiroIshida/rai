/*  ------------------------------------------------------------------
    Copyright 2016 Marc Toussaint
    email: marc.toussaint@informatik.uni-stuttgart.de
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. This program is distributed without
    any warranty. See the GNU General Public License for more details.
    You should have received a COPYING file of the full GNU General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>
    --------------------------------------------------------------  */


#include "kinViewer.h"

#include <iomanip>

//===========================================================================

OrsViewer::OrsViewer(const char* varname, double beatIntervalSec, bool computeCameraView)
  : Thread("OrsViewer", beatIntervalSec),
    modelWorld(this, varname, (beatIntervalSec<0.)),
    modelCameraView(this, "modelCameraView"),
    modelDepthView(this, "modelDepthView"),
    computeCameraView(computeCameraView){
  if(beatIntervalSec>=0.) threadLoop(); else threadStep();
}

OrsViewer::~OrsViewer(){ threadClose(); }

void OrsViewer::open(){
  copy.gl(modelWorld.name);
}

void OrsViewer::step(){
  copy.gl().dataLock.writeLock();
  copy = modelWorld.get();
  copy.gl().dataLock.unlock();
  copy.gl().update(NULL, false, false, true);
  if(computeCameraView){
    mlr::Shape *kinectShape = copy.getShapeByName("endeffKinect");
    if(kinectShape){ //otherwise 'copy' is not up-to-date yet
      copy.gl().dataLock.writeLock();
      mlr::Camera cam = copy.gl().camera;
      copy.gl().camera.setKinect();
      copy.gl().camera.X = kinectShape->X * copy.gl().camera.X;
//      openGlLock();
      copy.gl().renderInBack(true, true, 580, 480);
//      copy.glGetMasks(580, 480, true);
//      openGlUnlock();
      modelCameraView.set() = copy.gl().captureImage;
      modelDepthView.set() = copy.gl().captureDepth;
      copy.gl().camera = cam;
      copy.gl().dataLock.unlock();
    }
  }
}

//===========================================================================

void OrsPathViewer::setConfigurations(const WorldL& cs){
  configurations.writeAccess();
  listResize(configurations(), cs.N);
  for(uint i=0;i<cs.N;i++) configurations()(i)->copy(*cs(i), true);
  configurations.deAccess();
}

void OrsPathViewer::clear(){
  listDelete(configurations.set()());
}

OrsPathViewer::OrsPathViewer(const char* varname, double beatIntervalSec, int tprefix)
  : Thread("OrsPathViewer", beatIntervalSec),
    configurations(this, varname, (beatIntervalSec<0.)),
    t(0), tprefix(tprefix), writeToFiles(false){}

OrsPathViewer::~OrsPathViewer(){ threadClose(); clear(); }

void OrsPathViewer::open(){
  copy.gl(configurations.name);
}

void OrsPathViewer::step(){
  copy.gl().dataLock.writeLock();
  configurations.readAccess();
  uint T=configurations().N;
  if(t>=T) t=0;
  if(T) copy.copy(*configurations()(t), true);
  configurations.deAccess();
  copy.gl().dataLock.unlock();
  if(T){
    copy.gl().captureImg=writeToFiles;
    copy.gl().update(STRING(" (time " <<tprefix+int(t) <<'/' <<tprefix+int(T) <<')').p, false, false, true);
    if(writeToFiles) write_ppm(copy.gl().captureImage,STRING("vid/z.path."<<std::setw(3)<<std::setfill('0')<<tprefix+int(t)<<".ppm"));
  }
  t++;
}

//===========================================================================

OrsPoseViewer::OrsPoseViewer(const char* modelVarName, const StringA& poseVarNames, double beatIntervalSec)
  : Thread("OrsPoseViewer", beatIntervalSec),
    modelWorld(this, modelVarName, false),
    gl(STRING("OrsPoseViewer:" <<poseVarNames)){
  for(const String& varname: poseVarNames){
    poses.append( new Access_typed<arr>(this, varname, (beatIntervalSec<0.)) ); //listen only when beatInterval=1.
    copies.append( new mlr::KinematicWorld() );
  }
  copy = modelWorld.get();
  computeMeshNormals(copy.shapes);
  for(mlr::KinematicWorld *w: copies) w->copy(copy, true);
  if(beatIntervalSec>=0.) threadLoop();
}

OrsPoseViewer::~OrsPoseViewer(){
  threadClose();
  listDelete(copies);
  listDelete(poses);
}

void OrsPoseViewer::recopyKinematics(const mlr::KinematicWorld& world){
  stepMutex.lock();
  if(&world) copy=world;
  else copy = modelWorld.get();
  computeMeshNormals(copy.shapes);
  for(mlr::KinematicWorld *w: copies) w->copy(copy, true);
  stepMutex.unlock();
}

void OrsPoseViewer::open() {
  gl.add(glStandardScene, 0);
  gl.camera.setDefault();

  for(uint i=0;i<copies.N;i++) gl.add(*copies(i));
  //  gl.camera.focus(0.6, -0.1, 0.65);
  //  gl.width = 1280;
  //  gl.height = 960;
}

void OrsPoseViewer::step(){
  listCopy(copies.first()->proxies, modelWorld.get()->proxies);
//  cout <<copy.proxies.N <<endl;
  gl.dataLock.writeLock();
  for(uint i=0;i<copies.N;i++){
    arr q=poses(i)->get();
    if(q.N==copies(i)->getJointStateDimension())
      copies(i)->setJointState(q);
  }
  gl.dataLock.unlock();
  gl.update(NULL, false, false, true);
}

void OrsPoseViewer::close(){
  gl.clear();
}

//===========================================================================

ComputeCameraView::ComputeCameraView(double beatIntervalSec, const char* modelWorld_name)
  : Thread("ComputeCameraView", beatIntervalSec),
    modelWorld(this, modelWorld_name, (beatIntervalSec<.0)),
    cameraView(this, "kinect_rgb"), //"cameraView"),
    cameraDepth(this, "kinect_depth"), //"cameraDepth"),
    cameraFrame(this, "kinect_frame"), //"cameraFrame"),
    getDepth(true){
  if(beatIntervalSec<0.) threadOpen();
  else threadLoop();
}

ComputeCameraView::~ComputeCameraView(){
  threadClose();
}

void ComputeCameraView::open(){
  gl.add(glStandardLight);
  gl.addDrawer(&copy);
}

void ComputeCameraView::close(){
  gl.clear();
}

void ComputeCameraView::step(){
  copy = modelWorld.get();
  copy.orsDrawJoints = copy.orsDrawMarkers = copy.orsDrawProxies = false;

  mlr::Shape *kinectShape = copy.getShapeByName("endeffKinect");
  if(kinectShape){ //otherwise 'copy' is not up-to-date yet
    gl.dataLock.writeLock();
    gl.camera.setKinect();
    gl.camera.X = kinectShape->X * gl.camera.X;
    gl.dataLock.unlock();
    gl.renderInBack(true, getDepth, 640, 480);
    flip_image(gl.captureImage);
    flip_image(gl.captureDepth);
    cameraView.set() = gl.captureImage;
    if(getDepth){
      floatA& D = gl.captureDepth;
      uint16A depth_image(D.d0, D.d1);
      for(uint i=0;i<D.N;i++){
        depth_image.elem(i)
            = (uint16_t) (gl.camera.glConvertToTrueDepth(D.elem(i)) * 1000.); // conv. from [m] -> [mm]
      }
      cameraDepth.set() = depth_image;
    }
    cameraFrame.set() = kinectShape->X;
  }
}

