#ifndef ICLWIDGET_2_H
#define ICLWIDGET_2_H

#include <QGLWidget>
#include <iclImgBase.h>
#include <QMutex>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QMutex>
#include <QApplication>
#include <iclConverter.h>
#include <iclPaintEngine.h>
#include <iclTypes.h>
#include <iclImageStatistics.h>
#include <iclMouseInteractionInfo.h>
#include <iclMouseInteractionReceiver.h>
#include "iclWidgetCaptureMode.h"

class QImage;

namespace icl{

  /** \cond */
  class PaintEngine;
  class OSDWidget;
  class OSD;
  class OSDButton;
  class ImgBase;
  class GLTextureMapBaseImage;
  class QImageConverter;
  /** \endcond */
  
 
  /// Class for openGL-based image visualization components \ingroup COMMON
  /** The ICLWidget class provide basic abilities for displaying ICL images (ImgBase) on embedded 
      Qt GUI components. Its is fitted out with a responsive OpenGL-Overlay On-Screen-Display (OSD),
      which can be used to adapt some ICLWidget specific settings, and which is partitioned into several
      sub menus:
      - <b>adjust</b> here one can define image adjustment parameters via sliders, like brightness and
        contrast. In addition, there are three different modes: <em>off</em> (no adjustments are 
        preformed), <em>manual</em> (slider values are exploited) and <em>auto</em> (brightness 
        and intensity are adapted automatically to exploit the full image range). <b>Note:</b> This time,
        no intensity adaptions are implemented at all.
      - <b>fitmode</b> Determines how the image is fit into the widget geometry (see enum 
        ICLWidget::fitmode) 
      - <b>channel selection</b> A slider can be used to select a single image channel that should be 
        shown as gray image 
      - <b>capture</b> Yet, just a single button is available to save the currently shown image into the
        local directory 
      - <b>info</b> shows information about the current image
      - <b>menu</b> settings for the menu itself, like the alpha value or the color (very useful!)
      
      The following code (also available in <em>ICLQt/examples/camviewer_lite.cpp</em> demonstrates how
      simple an ICLWidget can be used to create a simple USB Webcam viewer:
      \code
      
\#include <iclWidget.h>
\#include <iclDrawWidget.h>
\#include <iclPWCGrabber.h>
\#include <QApplication>
\#include <QThread>
\#include <iclTestImages.h>
using namespace icl;
using namespace std;


// we need a working thread, which allows us to grab images
// from the webcam asynchronously to Qts event loop

class MyThread : public QThread{               
public:
  MyThread(){
    widget = new ICLWidget(0);                // create the widget
    widget->setGeometry(200,200,640,480);     // set up its geometry
    widget->show();                           // show it
    start();                                  // start the working loop (this will call the
  }                                           //           run() function in an own thread)   
  ~MyThread(){
    exit();                                   // this will destroy the working thread
    msleep(250);                              // wait till it is destroyed   
    delete widget;
  }
  
  virtual void run(){
    PWCGrabber g(Size(320,240));              // create a grabber instance
    while(1){                                 // enter the working loop
      widget->setImage(g.grab());             // grab a new image from the grabber an give it to the widget
      widget->update();                       // force qt to update the widgets user interface
    }
  }
  private:
  ICLWidget *widget;                          // internal widget object
};


int main(int nArgs, char **ppcArg){
  QApplication a(nArgs,ppcArg);           // create a QApplication
  MyThread x;                             // create the widget and thread
  return a.exec();                        // start the QAppliations event loop
}
      
      \endcode

      When other things, like annotations should be drawn to the widget, you can either
      create a new class that inherits the ICLWidget class, and reimplement the function:
      \code
      virtual void customPaintEvent(PaintEngine *e);
      \endcode
      or, you can use the extended ICLDrawWidget class, which enables you to give 2D draw
      commands in image coordinates. (The last method is more recommended, as it should be
      exactly what you want!!!)
      @see ICLDrawWidget
  */
  class ICLWidget2 : public QGLWidget{
    Q_OBJECT
    public:

    class Data;    
    
    /// determines how the image is fit into the widget geometry
    enum fitmode{ 
      fmNoScale,  /**< the image is not scaled it is centered to the image rect */
      fmHoldAR,   /**< the image is scaled to fit into the image rect, but its aspect ratio is hold */
      fmFit,      /**< the image is fit into the frame ( this may change the images aspect ratio)*/
      fmZoom      /**< new mode where an image rect can be specified in the gui ... */
    };
    
    /// determines intensity adaption mode 
    enum rangemode { 
      rmOn = 1 ,  /**< range settings of the sliders are used */ 
      rmOff = 2 , /**< no range adjustment is used */
      rmAuto      /**< automatic range adjustment */ 
    };   
    
    /// creates a new ICLWidget within the parent widget
    ICLWidget2(QWidget *parent=0);
    
    /// destructor
    virtual ~ICLWidget2();

    /// GLContext initialization
    virtual void initializeGL();
    
    /// called by resizeEvent to adapt the current GL-Viewport
    virtual void resizeGL(int w, int h);
    
    /// draw function
    virtual void paintGL();

    /// drawing function for NO-GL fallback
    virtual void paintEvent(QPaintEvent *e);
    
    /// this function can be overwritten do draw additional misc using the given PaintEngine
    virtual void customPaintEvent(PaintEngine *e);

    virtual void setVisible(bool visible);

    /// sets the current fitmode
    void setFitMode(fitmode fm);
    
    /// sets the current rangemode
    void setRangeMode(rangemode rm);
    
    /// set up current brightness, contrast and intensity adaption values
    void setBCI(int brightness, int contrast, int intensity);

    
    /// returns the widgets size as icl::Size
    Size getSize() { return Size(width(),height()); }
    
    /// returns the current images size
    Size getImageSize();

    /// returns the rect, that is currently used to draw the image into
    Rect getImageRect();

    /** .
        /// returns the current fitmode
        fitmode getFitMode(){return m_eFitMode;}
        
        /// returns the current rangemode
        rangemode getRangeMode(){return m_eRangeMode;}
    **/

    /// returns a list of image specification string (used by the OSD)
    std::vector<std::string> getImageInfo();

    
    /// returns whether a "NULL-Image" is warned in the image (...)
    //    void setShowNoImageWarning(bool enabled=true){ m_bShowNoImageWarning=enabled; }
    
    //    bool getMenuEnabled() const { return m_menuEnabled; }

    //void setMenuEnabled(bool enabled);

    /*
        /// calls QObject::connect to establish an explicit connection
        void add(MouseInteractionReceiver *r){
        connect((ICLWidget*)this,SIGNAL(mouseEvent(MouseInteractionInfo*)),
        (MouseInteractionReceiver*)r,SLOT(mouseInteraction(MouseInteractionInfo*)));
        }
    */

    /// this function should be called to update the widget asyncronously from a working thread
    void updateFromOtherThread();
    
    /// overloaded event function processing special thread save update events
    virtual bool event ( QEvent * event ){
      ICLASSERT_RETURN_VAL(event,false);
      if(event->type() == QEvent::User){
        update();
        return true;
      }else{
        return QGLWidget::event(event);
      }
    } 
    
    /// returns current ImageStatistics struct (used by OSD)
    const ImageStatistics &getImageStatistics();

    public slots:
    /// sets up the current image
    void setImage(const ImgBase *image);

    signals:
    /// invoked when any mouse interaction was performed
    void mouseEvent(MouseInteractionInfo *info);


    public:
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void enterEvent(QEvent *e);
    virtual void leaveEvent(QEvent *e);
    virtual void resizeEvent(QResizeEvent *e);
    virtual void childChanged(int id, void *val);


    public slots:
    void showHideMenu();
    void setMenuEmbedded(bool embedded);

    void bciModeChanged(int modeIdx);
    void brightnessChanged(int val);
    void contrastChanged(int val);
    void intensityChanged(int val);
    
    void scaleModeChanged(int modeIdx);
    void currentChannelChanged(int modeIdx);

    private:
    

    class OutputBufferCapturer;
    
    Data *m_data;
    
    /// if parameters are changed in the gui
    void rebufferImageInternal();

    /// prepares the current MouseInteractionInfo struct for being emitted
    MouseInteractionInfo *updateMouseInfo(MouseInteractionInfo::Type type);

  };
  
}
#endif