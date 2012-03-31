/********************************************************************
**                Image Component Library (ICL)                    **
**                                                                 **
** Copyright (C) 2006-2010 CITEC, University of Bielefeld          **
**                         Neuroinformatics Group                  **
** Website: www.iclcv.org and                                      **
**          http://opensource.cit-ec.de/projects/icl               **
**                                                                 **
** File   : ICLIO/src/RSBImageOutput.cpp                           **
** Module : ICLIO                                                  **
** Authors: Christof Elbrechter                                    **
**                                                                 **
**                                                                 **
** Commercial License                                              **
** ICL can be used commercially, please refer to our website       **
** www.iclcv.org for more details.                                 **
**                                                                 **
** GNU General Public License Usage                                **
** Alternatively, this file may be used under the terms of the     **
** GNU General Public License version 3.0 as published by the      **
** Free Software Foundation and appearing in the file LICENSE.GPL  **
** included in the packaging of this file.  Please review the      **
** following information to ensure the GNU General Public License  **
** version 3.0 requirements will be met:                           **
** http://www.gnu.org/copyleft/gpl.html.                           **
**                                                                 **
** The development of this software was supported by the           **
** Excellence Cluster EXC 277 Cognitive Interaction Technology.    **
** The Excellence Cluster EXC 277 is a grant of the Deutsche       **
** Forschungsgemeinschaft (DFG) in the context of the German       **
** Excellence Initiative.                                          **
**                                                                 **
*********************************************************************/

#include <rsb/Factory.h>
#include <rsb/converter/Repository.h>
#include <rsb/converter/ProtocolBufferConverter.h>

#include <ICLCore/ImageSerializer.h>

#include <ICLIO/ImageCompressor.h>
#include <ICLIO/RSBImageOutput.h>
#include <ICLIO/RSBImage.pb.h>

using namespace boost;
using namespace rsb;
using namespace rsb::converter;

namespace icl{

  struct StaticRSBImageTypeRegistration{
    StaticRSBImageTypeRegistration(){
      shared_ptr<ProtocolBufferConverter<RSBImage> > p(new ProtocolBufferConverter<RSBImage>());
      stringConverterRepository()->registerConverter(p);
    }
  } static_RSBImage_type_registration;
  
  struct RSBImageOutput::Data{
    Informer<RSBImage>::Ptr informer;
    Informer<RSBImage>::DataPtr out;
    std::string compressionType;
    std::string compressionQuality;
    ImageCompressor compressor;
  };

  RSBImageOutput::RSBImageOutput():m_data(0){}

  RSBImageOutput::~RSBImageOutput(){
    ICL_DELETE(m_data);
  }
    
  RSBImageOutput::RSBImageOutput(const std::string &scope, const std::string &transportList):m_data(0){
    init(scope,transportList);
  }

  void RSBImageOutput::setCompression(const std::string &compression, const std::string &quality){
    ICLASSERT_RETURN(!isNull());
    m_data->compressionType = compression;
    m_data->compressionQuality = quality;
  }
    
  std::pair<std::string,std::string> RSBImageOutput::getCompression() const{
    ICLASSERT_RETURN_VAL(!isNull(), (std::pair<std::string,std::string>()));
    return std::pair<std::string,std::string>(m_data->compressionType,m_data->compressionQuality);
  }
  
  void RSBImageOutput::init(const std::string &scope, const std::string &transportList){
    ICL_DELETE(m_data);
    m_data = new Data;
    m_data->compressionType = "off";
    m_data->compressionQuality = "default";
    
    Scope rsbScope(scope);
    ParticipantConfig rsbCfg;
    std::vector<std::string> transports = tok(transportList,",");
    for(size_t i=0;i<transports.size();++i){
      rsbCfg.addTransport(ParticipantConfig::Transport(transports[i]));
    }
    m_data->informer = Factory::getInstance().createInformer<RSBImage>(rsbScope,rsbCfg);
    m_data->out = Informer<RSBImage>::DataPtr(new RSBImage);
  }
    
  void RSBImageOutput::send(const ImgBase *image){
    ICLASSERT_RETURN(!isNull());
    ICLASSERT_RETURN(image->getDim() > 0);
    ICLASSERT_RETURN(image->getChannels() > 0);

    Informer<RSBImage>::DataPtr &out = m_data->out;
    
    out->set_width(image->getWidth());
    out->set_height(image->getHeight());
    out->set_channels(image->getChannels());
    out->set_time(image->getTime().toMicroSeconds());
    out->set_roix(image->getROIXOffset());
    out->set_roiy(image->getROIYOffset());
    out->set_roiw(image->getROIWidth());
    out->set_roih(image->getROIHeight());

    switch(image->getFormat()){
      case formatGray: out->set_format(RSBImage::formatGray); break;
      case formatRGB: out->set_format(RSBImage::formatRGB); break;
      case formatHLS: out->set_format(RSBImage::formatHLS); break;
      case formatYUV: out->set_format(RSBImage::formatYUV); break;
      case formatLAB: out->set_format(RSBImage::formatLAB); break;
      case formatChroma: out->set_format(RSBImage::formatChroma); break;
      case formatMatrix: out->set_format(RSBImage::formatMatrix); break;
    }
    
    // compression // set up in this class
    out->set_compressionmode(m_data->compressionType);
    out->set_compressionquality(m_data->compressionQuality);
    
    if(m_data->compressionType == "off"){
      // no compression:
      int len = ImageSerializer::estimateSerializedSize(image);
      std::vector<icl8u> buf(len);
      //    std::string *data = out->mutable_data();
      //data->resize(2*len);
      ImageSerializer::serialize(image,buf);//(icl8u*)&data[0]);
      out->set_data(buf.data(),len);
    }else{
      if(image->getDepth() != depth8u) {
        throw ICLException(( "RSBImageOutput::send: invalid image depth for image "
                             "compression (compression is only supported for 8u images)"));
      }
      if(m_data->compressionType == "rle"){
        if(m_data->compressionQuality == "1" || m_data->compressionQuality == "default"){
          m_data->compressor.setCompressionMode(ImageCompressor::CompressRLE1Bit);
        }else if(m_data->compressionQuality == "4"){
          m_data->compressor.setCompressionMode(ImageCompressor::CompressRLE4Bit);
        }else if(m_data->compressionQuality == "6"){
          m_data->compressor.setCompressionMode(ImageCompressor::CompressRLE6Bit);
        }else{
          throw ICLException("RSBImageOutput::send: invalid rle compression quality");
        }
      }else if(m_data->compressionType == "jpeg"){
        m_data->compressor.setCompressionMode(ImageCompressor::CompressJPEG);
        int quality = m_data->compressionQuality == "default" ? 90 : parse<int>(m_data->compressionQuality);
        if(quality <= 0 || quality > 100) {
          throw ICLException("RSBImageOutput::send: invalid jepg compression quality");
        }
        m_data->compressor.setJPEGQuality(quality);
      }else{
        throw ICLException("RSBImageOutput::send: invalid compression type");
      }
      ImageCompressor::CompressedData cmp = m_data->compressor.encode(*image->as8u());
      out->set_data(cmp.bytes,cmp.len);
    }
    //  std::string *metadata = out->mutable_metadata();
    m_data->informer->publish(out);

  }
};