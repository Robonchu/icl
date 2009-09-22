#include <iclCommon.h>
#include <iclScene2.h>
#include <iclExtrinsicCameraCalibrator.h>
#include <iclCommon.h>
#include <iclWarpOp.h>
#include <iclDynMatrix.h>

#include <QPushButton>
#include <iclLocalThresholdOp.h>
#include <iclRegionDetector.h>
#include <iclMorphologicalOp.h>
#include <iclCC.h>

#include <iclSOM2D.h>
#include <iclMathematics.h>
#include <QMessageBox>
#include <QFileDialog>
#include <iclPoint32f.h>
#include <iclConfigFile.h>
#include <iclGeomDefs.h>

#include <fstream>

enum CAM_TYPE{ VIEW_CAM, CALIB_CAM};
GUI gui("hsplit");
SOM2D *som = 0;
float focalLength;
int gridW(0),gridH(0);
std::string orientation;
typedef FixedColVector<float,3> Vec3;
std::vector<Vec> worldCoords;
GUI sceneGUI("hsplit");
Scene2 scene;
GenericGrabber *grabber = 0;
ImgParams imageParams;
Vec worldOffset(0,0,0,0);

struct MaskRect{
  Mutex mutex;
  Point origin;
  Point curr;
  Rect rect;
  Img8u maskedImage;
  void draw(ICLDrawWidget &w, int imagew, int imageh){
    Mutex::Locker l(mutex);
    if(rect != Rect::null ){
      w.color(0,100,255,255);
      w.fill(0,0,0,0);
      w.rect(rect.x,rect.y,rect.width,rect.height);

      w.color(0,0,0,0);
      w.fill(0,100,255,150);
      w.rect(0,0,rect.x,imageh);
      w.rect(rect.right(),0,imagew-rect.right(),imageh);
      w.rect(rect.x,rect.bottom(),rect.width,imageh-rect.bottom());
      w.rect(rect.x,0,rect.width,rect.y);
    }
    if(curr != Point::null && origin != Point::null){
      w.color(0,255,40,255);
      w.fill(0,0,0,0);
      Rect r(origin, Size(curr.x-origin.x,curr.y-origin.y ));
      w.rect(r.x,r.y,r.width,r.height);
    }
  }
  const Img8u &applyMask(const Img8u &src){
    Mutex::Locker l(mutex);
    if(rect == Rect::null) return src;
    maskedImage.setParams(src.getParams());
    maskedImage.clear();
    Img8u srcCpy(src);
    srcCpy.setROI(rect & srcCpy.getImageRect());
    maskedImage.setROI(rect & srcCpy.getImageRect());
    srcCpy.deepCopyROI(&maskedImage);
    return maskedImage;
  }
} maskRect;

struct MaskRectMouseHandler : public MouseHandler{
  
  virtual void process(const MouseEvent &e){
    Mutex::Locker l(maskRect.mutex);
    if(e.isRight()){
      maskRect.rect = Rect::null;
      return;
    }
    if(e.isPressEvent()){
      maskRect.origin  = e.getPos();
      maskRect.curr   = e.getPos();
    }else if(e.isDragEvent()){
      maskRect.curr   = e.getPos();
    }
    if(e.isReleaseEvent()){
      if(maskRect.origin != maskRect.curr){
        maskRect.rect = Rect(maskRect.origin, Size(maskRect.curr.x-maskRect.origin.x,
                                                   maskRect.curr.y-maskRect.origin.y ));
        maskRect.rect = maskRect.rect.normalized();
        if(maskRect.rect.getDim() < 4) {
          maskRect.rect = Rect::null;
        }
      }
      maskRect.origin = Point::null;
      maskRect.curr = Point::null;
    }
  }
};

void apply_calib(const std::vector<Point32f> &cogs, const std::vector<int> &ass, const Size &imageSize){
  // {{{ open

  std::vector<Point32f> cogsSorted(cogs.size());
  for(int x=0;x<gridW;++x){
    for(int y=0;y<gridH;++y){
      int idx = x+gridW*y;
      int a = ass[idx];
      const Point32f &p = cogs[a];
      //      const Vec &v = worldCoords[idx];
      cogsSorted[idx] = p;
    }
  }
  
  enum {LINEAR,LIN_AND_STOCHASTIC_SIMPLE,LIN_AND_STOCHASTIC_COMLPLEX,LIN_AND_STOCHASTIC_SUPER};
  static ComboHandle &calibMode = gui.getValue<ComboHandle>("calib-mode");
  static bool &calibFocalLength = gui.getValue<bool>("calib-focal-length-val");
  static bool &useAnnealing = gui.getValue<bool>("use-annealing-val");
  static LabelHandle &currError = gui.getValue<LabelHandle>("curr-error");

  ExtrinsicCameraCalibrator::Result calibrationResult;
  switch(calibMode.getSelectedIndex()){
    case LINEAR: 
      calibrationResult = ExtrinsicCameraCalibrator::calibrateLinear(worldCoords,cogsSorted,
                                                                     imageSize,focalLength);
      break;
    case LIN_AND_STOCHASTIC_SIMPLE: 
      calibrationResult = ExtrinsicCameraCalibrator::calibrateLinearAndStochastic(worldCoords,cogsSorted,
                                                                                  imageSize,focalLength,
                                                                                  1000,0.4,calibFocalLength,
                                                                                  useAnnealing);
      break;
    case LIN_AND_STOCHASTIC_COMLPLEX:
      calibrationResult = ExtrinsicCameraCalibrator::calibrateLinearAndStochastic(worldCoords,cogsSorted,
                                                                                  imageSize,focalLength,
                                                                                  10000,0.4,calibFocalLength,
                                                                                  useAnnealing);
      break;
    case LIN_AND_STOCHASTIC_SUPER:
      calibrationResult = ExtrinsicCameraCalibrator::calibrateLinearAndStochastic(worldCoords,cogsSorted,
                                                                                  imageSize,focalLength,
                                                                                  100000,0.4,calibFocalLength,
                                                                                  useAnnealing);
      break;
    default:
      ERROR_LOG("what have you done to the combobox?");
  }
  scene.getCamera(CALIB_CAM) = calibrationResult.camera;
  scene.getCamera(CALIB_CAM).setZFar(10000);
  scene.getCamera(CALIB_CAM).setZNear(1.0/10000);
  currError = calibrationResult.error;
  
  //  scene.getCamera(CALIB_CAM).show("calib camera");
}

// }}}

Rect32f find_bounding_box(const std::vector<Point32f> &ps){
  // {{{ open

    if(!ps.size()) return Rect32f();
    Point ul=ps[0];
    Point lr=ps[0];
    for(unsigned int i=1;i<ps.size();++i){
      const float &x = ps[i].x;
      const float &y = ps[i].y;
      
      if(ul.x > x) ul.x = x;
      if(ul.y > y) ul.y = y;
      if(lr.x < x) lr.x = x;
      if(lr.y < y) lr.y = y;
    }
    return Rect32f(ul.x,ul.y,lr.x-ul.x,lr.y-ul.y);
  }

// }}}

std::vector<int> associate(const std::vector<Point32f> &cogs,const Size &imageSize){
  // {{{ open



  if(gridW*gridH != (int)cogs.size()){
    return std::vector<int>();
  }
  
  std::vector<int> ass(cogs.size());
  static std::vector<Range32f> initBounds(2,Range32f(0,1));
  randomSeed();

  static const float E_start = 0.8;
  
#ifdef USE_OLD_SOM_INITIALIZATION
  int imageW(imageSize.width),imageH(imageSize.height);
  static const float M = 10;         // prototype initialization pixel-margin
  float mx = float(imageW-2*M)/(gridW-1);
  float my = float(imageH-2*M)/(gridH-1);
  float bx = M;
  float by = M;
#else
  // NEW: find cogs' bounding box for SOM initialization
  Rect32f bb = find_bounding_box(cogs);
  float mx = float(bb.width)/(gridW-1);
  float my = float(bb.height)/(gridH-1);
  float bx = bb.x;
  float by = bb.y;

#endif
  if(!::som){
    ::som = new SOM2D(2,gridW,gridH,initBounds,0.5,0.5);
  }
  SOM2D &som = *::som;
  for(int x=0;x<gridW;++x){
    for(int y=0;y<gridH;++y){
      float *p = som.getNeuron(x,y).prototype;
      p[0] = mx * x + bx;
      p[1] = my * y + by;
    }
  }

  URandI ridx(cogs.size()-1);
  float buf[2];

  static int numSomTrainingSteps = pa_subarg<int>("-som-training-steps",0,100);
  static float somDecayFactor = pa_subarg<float>("-som-decay-factor",0,5);
  static float somNeighbourhoodFactor = pa_subarg<float>("-som-nbh-factor",0,1);

  for(int j=0;j<numSomTrainingSteps;++j){
    float relativeStep = float(j)/float(numSomTrainingSteps);
    som.setEpsilon(E_start * ::exp(-relativeStep*somDecayFactor));
    som.setSigma(somNeighbourhoodFactor);
    for(int i=0;i<100;++i){
      unsigned int ridxVal = ridx; 
      ICLASSERT(ridxVal < cogs.size());
      const Point32f &rnd = cogs[ridxVal];
      buf[0] = rnd.x;
      buf[1] = rnd.y;
      som.train(buf);
    }
  }
  
  for(unsigned int i=0;i<cogs.size();++i){
    const Point32f &p = cogs[i];
    buf[0] = p.x;
    buf[1] = p.y;
    const float *g = som.getWinner(buf).gridpos;
    int x = ::round(g[0]);
    int y = ::round(g[1]);
    int idx = x + gridW * y;
    if(idx >=0 && idx < gridW*gridH){
      ass[idx] = i;
    }else{
      return std::vector<int>();
    }
  }
  return ass;
}

// }}}

void vis_som(SOM2D  &som,ICLDrawWidget &w){
  // {{{ open

  w.color(255,0,0,100);
  ImgQ ps = zeros(gridW,gridH,2);
  
  for(int x=0;x<gridW;++x){
    for(int y=0;y<gridH;++y){
      float *p = som.getNeuron(x,y).prototype;
      ps(x,y,0) = p[0];
      ps(x,y,1) = p[1];
    }
  }
  for(int x=1;x<gridW;++x){
    for(int y=1;y<gridH;++y){
      w.line(ps(x,y,0),ps(x,y,1),ps(x-1,y,0),ps(x-1,y,1));
      w.line(ps(x,y,0),ps(x,y,1),ps(x,y-1,0),ps(x,y-1,1));
    }
  }
  for(int x=1;x<gridW;++x){
    w.line(ps(x,0,0),ps(x,0,1),ps(x-1,0,0),ps(x-1,0,1));
  }
  for(int y=1;y<gridH;++y){
    w.line(ps(0,y,0),ps(0,y,1),ps(0,y-1,0),ps(0,y-1,1));
  }
}

// }}}

void vis_ass(const std::vector<Point32f> &cogs, const std::vector<int> &ass,ICLDrawWidget &w){
  // {{{ open

  w.color(255,0,0,255);
  for(int x=0;x<gridW;++x){
    for(int y=0;y<gridH;++y){
      int idx = x+gridW*y;
      int a = ass[idx];
      const Point32f &p = cogs[a];
      const Vec &v = worldCoords[idx];
      w.text(str(idx),p.x-10,p.y,-1,-1,10);
      w.text(str(v.transp()),p.x,p.y,-1,-1,8);
    }
  }
  
  if(orientation == "horz"){

    for(int x=0;x<gridW-1;++x){
      w.color(255,0,0,255);
      for(int y=0;y<gridH/2-1;++y){
        const Point32f &pxy = cogs[ass[x+gridW*y]];
        const Point32f &pr = cogs[ass[x+1+gridW*y]];
        const Point32f &pl = cogs[ass[x+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }
      {
        const Point32f &pxy = cogs[ass[x+gridW*(gridH/2-1)]];
        const Point32f &pr = cogs[ass[x+1+gridW*(gridH/2-1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
      }

      w.color(0,100,255,255);
      for(int y=gridH/2;y<gridH-1;++y){
        const Point32f &pxy = cogs[ass[x+gridW*y]];
        const Point32f &pr = cogs[ass[x+1+gridW*y]];
        const Point32f &pl = cogs[ass[x+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }
      {
        const Point32f &pxy = cogs[ass[x+gridW*(gridH-1)]];
        const Point32f &pr = cogs[ass[x+1+gridW*(gridH-1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
      }
    }
    
    for(int y=0;y<gridH/2-1;++y){
      w.color(255,0,0,255);
      const Point32f &pxy = cogs[ass[gridW-1+gridW*y]];
      const Point32f &pl = cogs[ass[gridW-1+gridW*(y+1)]];
      w.line(pxy.x,pxy.y,pl.x,pl.y);
    }
    for(int y=gridH/2;y<gridH-1;++y){
      w.color(0,100,255,255);
      const Point32f &pxy = cogs[ass[gridW-1+gridW*y]];
      const Point32f &pl = cogs[ass[gridW-1+gridW*(y+1)]];
      w.line(pxy.x,pxy.y,pl.x,pl.y);
    }

  }else{
    /// todo
    for(int y=0;y<gridH-1;++y){
      w.color(255,0,0,255);
      for(int x=0;x<gridW/2-1;++x){
        const Point32f &pxy = cogs[ass[x+gridW*y]];
        const Point32f &pr = cogs[ass[x+1+gridW*y]];
        const Point32f &pl = cogs[ass[x+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }
      {
        const Point32f &pxy = cogs[ass[gridW/2-1+gridW*y]];
        const Point32f &pl = cogs[ass[gridW/2-1+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }

      w.color(0,100,255,255);
      for(int x=gridW/2;x<gridW-1;++x){
        const Point32f &pxy = cogs[ass[x+gridW*y]];
        const Point32f &pr = cogs[ass[x+1+gridW*y]];
        const Point32f &pl = cogs[ass[x+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pr.x,pr.y);
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }
      {
        const Point32f &pxy = cogs[ass[gridW-1+gridW*y]];
        const Point32f &pl = cogs[ass[gridW-1+gridW*(y+1)]];
        w.line(pxy.x,pxy.y,pl.x,pl.y);
      }
    }
    
    for(int x=0;x<gridW/2-1;++x){
      w.color(255,0,0,255);
      const Point32f &pxy = cogs[ass[x+gridW*(gridH-1)]];
      const Point32f &pr = cogs[ass[x+1+gridW*(gridH-1)]];
      w.line(pxy.x,pxy.y,pr.x,pr.y);
    }

    for(int x=gridW/2;x<gridW-1;++x){
      w.color(0,100,255,255);
      const Point32f &pxy = cogs[ass[x+gridW*(gridH-1)]];
      const Point32f &pr = cogs[ass[x+1+gridW*(gridH-1)]];
      w.line(pxy.x,pxy.y,pr.x,pr.y);
    }
  }
}

// }}}

void show_hide_scene(){
  // {{{ open

  if(sceneGUI.getRootWidget()->isVisible()){
    sceneGUI.getRootWidget()->hide();
  }else{
    sceneGUI.show();
  }
}

// }}}

void add_cuboid(float a, float b, float c, float d, float e, float f, const GeomColor &col){
  // {{{ open

  const float p[] = { a,b,c,d,e,f };
  Object2 *obj = new Object2("cuboid",p);
  obj->setColor(Primitive::quad,col);
  obj->setVisible(Primitive::line,false);
  scene.addObject(obj);
}

// }}}

void init_scene_and_scene_gui(){
  // {{{ open
  
  std::string size = str(imageParams.getSize()/20);
  sceneGUI << "draw3D[@minsize=16x12@handle=scene]";
  sceneGUI << ("draw3D[@handle=calib-scene@minsize="+size+"@maxsize="+size+"]");
  sceneGUI.create();

  scene.addCamera(Camera(Vec(-500,-320,570),
                         Vec(0.879399,0.169548,-0.444871),
                         Vec(0.339963,0.263626,0.902733),
                         Size::VGA,
                         6,
                         0.0001,10000));
  scene.addCamera(Camera(Vec(0.0),Vec(0.0),imageParams.getSize(),focalLength));

  ICLDrawWidget3D &w = **sceneGUI.getValue<DrawHandle3D>("scene");
  w.setImageInfoIndicatorEnabled(false);
  ICLDrawWidget3D &cw = **sceneGUI.getValue<DrawHandle3D>("calib-scene");
  cw.setImageInfoIndicatorEnabled(false);

  static const float s = 0.5;
  static const float l = 100;
  static const float l2 = l/2;
  add_cuboid(0,0,0,s,s,s,GeomColor(255,255,0,255));
  add_cuboid(l2,0,0,l,s,s,GeomColor(255,0,0,255));
  add_cuboid(0,l2,0,s,l,s,GeomColor(0,255,0,255));
  add_cuboid(0,0,l2,s,s,l,GeomColor(0,100,255,255));



  // add calibration pattern
  for(unsigned int i=0;i<worldCoords.size();++i){
    GeomColor col;
    if(orientation == "horz"){
      col = i < worldCoords.size()/2 ? GeomColor(255,0,0,255) : GeomColor(0,100,255,255);
    }else{
      col = int(i)%(gridW)>=gridW/2 ? GeomColor(255,0,0,255) : GeomColor(0,100,255,255);
    }
    const Vec &p = worldCoords[i];
    add_cuboid(p[0],p[1],p[2],5,5,5,col);
  }

  w.install(scene.getMouseHandler(VIEW_CAM));  
  cw.install(scene.getMouseHandler(CALIB_CAM));  
}

// }}}

void run_scene(){
  // {{{ open
  
  static ICLDrawWidget3D *ws[2] = {
    *sceneGUI.getValue<DrawHandle3D>("scene"),
    *sceneGUI.getValue<DrawHandle3D>("calib-scene")
  };

  for(int i=0;i<2;++i){
    ws[i]->lock();

    ws[i]->reset3D();
    ws[i]->callback(scene.getGLCallback(i));

#if 0
    ws[i]->reset();
    ws[i]->color(0,100,255,255);
    ws[i]->fill(0,100,255,100);
    for(unsigned int j=0;j<worldCoords.size();++j){
      worldCoords[j][3]=1;
      Point32f p = scene.getCamera(i).project(worldCoords[j]);
      ws[i]->ellipse(p.x-5,p.y-10,20,20);
    }
#endif
    
    ws[i]->unlock();
    ws[i]->updateFromOtherThread();
  }
  
  if(pa_defined("-show-cam")){
    scene.getCamera(VIEW_CAM).show("view-cam");
  }
}

// }}}

void init(){
  // {{{ open
  if(pa_defined("-create-empty-config-file")){
    std::ofstream of("new-calib-config.xml");
    of << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" << std::endl
       << "<config>" << std::endl
       << "  <title>Camera Calibration Config-File</title>" << std::endl
       << "  <data id=\"world-offset\" type=\"string\">0,0,0</data>" << std::endl
       << "  <section id=\"calibration-object\">" << std::endl
       << "      <data id=\"nx\" type=\"int\">4</data>" << std::endl
       << "      <data id=\"ny\" type=\"int\">3</data>" << std::endl
       << "      <data id=\"orientation\" type=\"string\">horz</data>" << std::endl
       << "    <section id=\"wall-1\">" << std::endl
       << "      <data id=\"offset\" type=\"string\">35 204 241</data>" << std::endl
       << "      <data id=\"direction-1\" type=\"string\">71 0 0</data>" << std::endl
       << "      <data id=\"direction-2\" type=\"string\">0 0 -71</data>" << std::endl
       << "    </section>" << std::endl
       << "    <section id=\"wall-2\">" << std::endl
       << "      <data id=\"offset\" type=\"string\">37 169 64</data>" << std::endl
       << "      <data id=\"direction-1\" type=\"string\">71 0 0</data>" << std::endl
       << "      <data id=\"direction-2\" type=\"string\">0 -71 0</data>" << std::endl
       << "    </section>" << std::endl
       << "  </section>" << std::endl
       << "</config>" << std::endl;
    exit(0);
  }
  try{
    ConfigFile::loadConfig(pa_subarg<std::string>("-config",0,"calib-config.xml"));
  }catch(FileNotFoundException &ex){
    ERROR_LOG("config file not found: " << ex.what());
    std::cout << "try calling this application with -create-empty-config-file to generate a config file skeleton for you" << std::endl;
    exit(-1);
  }catch(InvalidFileFormatException &ex){
    ERROR_LOG("ivalid config file file-format: " << ex.what());
    exit(-1);
  } 

  if(!pa_defined("-focal-length")){
    pa_usage("please define -focal-length arg!");
    exit(-1);
  }
  gui << "draw[@minsize=16x12@label=main view@handle=mainview]";

  GUI controls("vbox");
  controls << ( GUI("hbox") 
                <<  "combo(!color,gray,thresh,morph)[@out=vis@label=visualization]"
                << "togglebutton(off,!on)[@out=grab-loop-val@handle=grab-loop@label=grab loop]" );
  
  controls << ( GUI("hbox") 
                << "togglebutton(off,!on)[@out=vis-overlay-val@handle=vis-overlay@label=vis. overlay]"
                << "togglebutton(off,on)[@out=som-on-val@handle=som-on@label=som visual.]" );
  
  controls << ( GUI("hbox") 
                << "togglebutton(off,on)[@out=ass-vis-on-val@handle=ass-vis-on@label=association visualiz.]"
                << "togglebutton(off,on)[@out=calibrate-on-val@handle=calibrate-on@label=calibration]" );
  
  controls << ( GUI("hbox") 
                << "combo(linear,linear+stochastic - simple,linear+stochastic - complex"
                   ",linear+stochastic - super)[@out=calib-mode-out@handle=calib-mode@label=calibration mode]"
                << "togglebutton(off,on)[@out=calib-focal-length-val@label=calib. f.-length]" );
                
  controls << ( GUI("hbox") 
                << "togglebutton(off,on)[@out=use-annealing-val@label=use annealing]"
                << "label(...)[@handle=curr-error@label=current error]" );
  
  controls << "fslider(0.6,2 .0,1.5)[@out=min-form-factor@label=roundness]";
  controls << "slider(10,10000,50)[@out=min-blob-size@label=min blob size]";
  controls << "slider(10,100000,1000)[@out=max-blob-size@label=max blob size]";
  controls << (GUI("vbox[@label=local threshold]") 
               << "slider(2,100,10)[@out=mask-size@label=mask size]"
               << "slider(-20,20,-10)[@out=thresh@label=threshold]");
  controls << (GUI("hbox") 
               << "button(show/hide scene)[@handle=show-hide-scene]"
               << "button(show camera)[@handle=show-camera]"
               );

  gui << controls;
  
  gui.show();
  
  gui.registerCallback(new GUI::Callback(show_hide_scene),"show-hide-scene");
  (*gui.getValue<DrawHandle>("mainview"))->install(new MaskRectMouseHandler);
  
  focalLength = pa_subarg<float>("-focal-length",0,0);

  gridW = ConfigFile::sget<int>("config.calibration-object.nx");
  gridH = ConfigFile::sget<int>("config.calibration-object.ny");
  orientation = ConfigFile::sget<std::string>("config.calibration-object.orientation");
  if(pa_defined("-orientation")){
    std::string pa_orientation = pa_subarg<std::string>("-orientation",0,"horz");
    if(pa_orientation != orientation){
      std::cout << "Warning: overwriting config files orientation ( " << orientation 
                <<  " )  with programm arguemnt value (" << pa_orientation << " )\n";
      orientation = pa_orientation;
    }
  }
  
  Vec3 w1o = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-1.offset"));
  int w1o_idx = ConfigFile::sget<int>("config.calibration-object.wall-1.offset-idx");
  Vec3 w1d1 = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-1.direction-1"));  
  Vec3 w1d2 = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-1.direction-2"));  

  Vec3 w2o = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-2.offset"));
  int w2o_idx = ConfigFile::sget<int>("config.calibration-object.wall-2.offset-idx");
  Vec3 w2d1 = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-2.direction-1"));  
  Vec3 w2d2 = parse<Vec3>(ConfigFile::sget<std::string>("config.calibration-object.wall-2.direction-2"));  
  
  Vec3 wo = parse<Vec3>(ConfigFile::sget<std::string>("config.world-offset"));
  worldOffset = Vec(wo[0],wo[1],wo[2],0);
  //TODO_LOG("worldOffset is currently ignored! (worldOffset was: " << worldOffset.transp() << ")");
  
#define USE_SPECIAL_OFFSET_CALCULATION
#ifdef USE_SPECIAL_OFFSET_CALCULATION
  if(orientation == "horz"){
    // WALL i)
    float x = w1o_idx % gridW;
    float y = w1o_idx / gridW;
    w1o -= w1d1 * x + w1d2 *y;

    // WALL ii)
    w2o_idx -= gridW*gridH;
    x = w2o_idx % gridW;
    y = w2o_idx / gridW;
    w2o -= w2d1 * x + w2d2 *y;
  }else{
    // WALL i)
    float x = w1o_idx % (2*gridW);
    float y = w1o_idx / (2*gridW);
    w1o -= w1d1 * x + w1d2 *y;

    // WALL ii)
    x = w2o_idx % (2*gridW) - gridW;
    y = w2o_idx / (2*gridW);
    w2o -= w2d1 * x + w2d2 *y;
  }
  
#endif
  if(orientation == "horz"){
    for(int y=0;y<gridH;++y){
      for(int x=0;x<gridW;++x){
        Vec3 v = w1o + w1d1*x + w1d2*y;
      worldCoords.push_back(Vec(v[0],v[1],v[2]));
      }
    }
    for(int y=0;y<gridH;++y){
      for(int x=0;x<gridW;++x){
        Vec3 v = w2o + w2d1*x + w2d2*y;
        worldCoords.push_back(Vec(v[0],v[1],v[2]));
      }
    }
  }else{ // vert case ...
    for(int y=0;y<gridH;++y){
      for(int x=0;x<gridW;++x){
        Vec3 v = w1o + w1d1*x + w1d2*y;
        worldCoords.push_back(Vec(v[0],v[1],v[2]));
      }
      for(int x=0;x<gridW;++x){
        Vec3 v = w2o + w2d1*x + w2d2*y;
        worldCoords.push_back(Vec(v[0],v[1],v[2]));
      }
    }
  }
    

  if(orientation == "horz"){
    gridH *= 2;
  }else if(orientation == "vert"){
    gridW *=2;
  }else{
    ERROR_LOG("orientation must be either horz or vert (asserting horz for now!)");
    gridH *= 2;
  }

  grabber = new GenericGrabber(FROM_PROGARG("-input"));
  grabber->setIgnoreDesiredParams(true);
  imageParams = grabber->grab()->getParams();
  if(pa_defined("-dist")){
    double f[4] = {
      pa_subarg<float>("-dist",0,0), pa_subarg<float>("-dist",1,0),
      pa_subarg<float>("-dist",2,0), pa_subarg<float>("-dist",3,0)
    };
    grabber->enableDistortion(f,imageParams.getSize());
  }


  init_scene_and_scene_gui();
  
}

// }}}

void run(){
  // {{{ open

  static DrawHandle &mainView = gui.getValue<DrawHandle>("mainview");
  static std::string &vis = gui.getValue<std::string>("vis");
  static bool &grab = gui.getValue<bool>("grab-loop-val");
  static bool &visOverlay = gui.getValue<bool>("vis-overlay-val");
  static bool &somOn = gui.getValue<bool>("som-on-val");
  static bool &assVisOn = gui.getValue<bool>("ass-vis-on-val");
  static bool &calibOn = gui.getValue<bool>("calibrate-on-val");

  static Img8u image;
  if(grab || !image.getDim()){
    grabber->grab()->convert(&image);
  }
  
  static Img8u grayIm(image.getSize(),formatGray);
  cc(&image,&grayIm);
  
  static LocalThresholdOp lt(35,-10,0);
  static int &threshold = gui.getValue<int>("thresh");
  static int &maskSize = gui.getValue<int>("mask-size");
  lt.setGlobalThreshold(threshold);
  lt.setMaskSize(maskSize);

  const ImgBase *ltIm = lt.apply(&grayIm);

  static MorphologicalOp morph(MorphologicalOp::dilate3x3);
  morph.setClipToROI(false);
  const ImgBase *moIm = morph.apply(ltIm);

  const Img8u &maskedImage = maskRect.applyMask(*moIm->asImg<icl8u>());

  static RegionDetector rd(100,50000,0,0);
  rd.setRestrictions(gui.getValue<int>("min-blob-size"),
                     gui.getValue<int>("max-blob-size"),0,0);
  const std::vector<icl::Region> &rsd = rd.detect(&maskedImage);
  std::vector<icl::Region> rs;
  std::vector<Point32f> cogs;
  std::vector<Point32f> accurate_cogs;
  for(unsigned int i=0;i<rsd.size();++i){
    static float &minFF = gui.getValue<float>("min-form-factor");
    if(rsd[i].getFormFactor() <= minFF){
      rs.push_back(rsd[i]);
      cogs.push_back(rsd[i].getCOG());
      accurate_cogs.push_back(rsd[i].getAccurateCOG(grayIm));
    }
  }
  
  
  std::vector<int> ass;
  if(somOn || assVisOn){
    ass = associate(cogs,image.getSize());

    if(!ass.size()){
      ERROR_LOG("unable to sort points!");
    }
  }
  

  if(vis == "color"){
    mainView = image;
  }else if (vis == "gray"){
    mainView = grayIm;
  }else if (vis == "thresh"){
    mainView = ltIm;
  }else if (vis == "morph"){
    mainView = moIm;
  }

  if(calibOn && ass.size()){
    apply_calib(cogs,ass,image.getSize());
  }
  
  ICLDrawWidget &w = **mainView;
  w.lock();
  w.reset();
  maskRect.draw(w,w.getImageSize().width,w.getImageSize().height);
  
  if(visOverlay){
    w.color(255,0,0);
    w.fill(255,0,0,50);
    w.symsize(10);
    for(unsigned int i=0;i<rs.size();++i){
      const icl::Region &r = rs[i];
      w.color(255,0,0);
      w.rect(r.getBoundingBox());
      w.sym(cogs[i].x,cogs[i].y,ICLDrawWidget::symPlus);
      w.color(0,255,0);
      w.sym(accurate_cogs[i].x,accurate_cogs[i].y,ICLDrawWidget::symCross);
    }
    
    w.color(255,0,0);
    if(somOn && ass.size()){
      vis_som(*som,w);
    }
    if(assVisOn && ass.size()){
      vis_ass(cogs,ass,w);
    }
  
  }
  w.unlock();
  mainView.update();

  if(sceneGUI.getRootWidget()->isVisible()){
    static DrawHandle3D &optView = sceneGUI.getValue<DrawHandle3D>("calib-scene");
    optView = &image;

    run_scene();
  }
  
  static ButtonHandle &showButton = gui.getValue<ButtonHandle>("show-camera");
  if(showButton.wasTriggered()){
    Camera c = scene.getCamera(CALIB_CAM);
    std::cout << "------------------------------------------------------" << std::endl;
    DEBUG_LOG("estimated camera pos is:" << c.getPos());
    DEBUG_LOG("worldOffset is:" << worldOffset);
    c.setPos(c.getPos()+worldOffset);
    std::string filename = pa_subarg<std::string>("-o",0,"extracted-cam-cfg.xml");
    std::cout << "new config file: (written to " <<  filename << ")" << std::endl;
    std::cout << c << std::endl;
    
    std::ofstream file(filename.c_str());
    file << c;
    std::cout << "------------------------------------------------------" << std::endl;
  }

  Thread::msleep(10);
}

// }}}

int main(int n, char **ppc){
  // {{{ open

  pa_explain("-input","define input device e.g. '-input dc 0' or '-input file *.ppm'");
  pa_explain("-focal-length","define focal length in mm (should be about 16-40 or something)\n[madatory]");
  pa_explain("-o","define output config xml file name (./extracted-camera-cfg.xml)");
  pa_explain("-orientation","one of horz or vert (orientation of the edge |: vertical  -: horizontal)\n"
             "\t(this can be used to overwrite orientation entry from the config file)");
  pa_explain("-show-cam","show current view-camera parameters on std::out");
  pa_explain("-config","define input marker config file (calib-config.xml by default) "
             "-show-cam");
  pa_explain("-som-training-steps","define count of SOM trainingsteps, that are used to associate markers (default 100)");
  pa_explain("-dist","give 4 distortion parameters");
  pa_explain("-som-nbh-factor","som epsilon (distance weighting in" 
             " neighbourhood function [see ICL::ICLAlgorithm::SOM class reference])");
  pa_explain("-som-decay-factor","som learning rate at relative time t (t=0..1) is exp(t*factor)");
  pa_explain("-create-empty-config-file","if this flag is given, an empty config file is created as ./new-calib-config.xml");
  ICLApplication app(n,ppc,"-focal-length(1) -input(2) -nx(1) -ny(1) -orientation(1) "
                     "-som-training-steps(1) -som-decay-factor(1) -som-nbh-factor(1) "
                     "-config(1) -dist(4) -create-empty-config-file -o(1)",init,run);
  
  
 

 
  return app.exec();
}

// }}}