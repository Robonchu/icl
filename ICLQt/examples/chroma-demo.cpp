#include <iclChromaGUI.h>
#include <iclCommon.h>

using namespace icl;
using namespace std;

GUI *gui;
ChromaGUI  *cg;

void run(){
  Size size = Size(320,240);
  GenericGrabber grabber(FROM_PROGARG("-input"));

  Img8u segImage(size,1);
  Img8u *image = new Img8u(size,formatRGB);
  ImgBase *imageBase = image;
  
  while(1){
    grabber.grab(&imageBase);
    gui->getValue<ImageHandle>("image") = image;
    gui->getValue<ImageHandle>("image").update();
    
    Channel8u c[3]; image->asImg<icl8u>()->extractChannels(c);
    Channel8u s = segImage.extractChannel(0);
    
    ChromaAndRGBClassifier classi = cg->getChromaAndRGBClassifier();
    
    for(int x=0;x<size.width;x++){
      for(int y=0;y<size.height;y++){
        s(x,y) = 255 * classi(c[0](x,y),c[1](x,y),c[2](x,y));
      }
    }
    
    gui->getValue<ImageHandle>("segimage") = &segImage;     
    gui->getValue<ImageHandle>("segimage").update();
    Thread::msleep(40);
  }
}

int main(int nArgs, char **ppcArgs){
  ExecThread x(run);  
  QApplication app(nArgs,ppcArgs);
  pa_init(nArgs, ppcArgs,"-input(2)");
  
  gui = new GUI("hbox");
  (*gui) << ( GUI("vbox")  
              << "image[@minsize=16x12@handle=image@label=Camera Image]" 
              << "image[@minsize=16x12@handle=segimage@label=Semented Image]" );
  (*gui) << "hbox[@handle=box]";
  
  gui->show();

  cg = new ChromaGUI(*gui->getValue<BoxHandle>("box"));

  x.run();
  
  return app.exec();
}
