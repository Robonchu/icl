/********************************************************************
**                Image Component Library (ICL)                    **
**                                                                 **
** Copyright (C) 2006-2012 CITEC, University of Bielefeld          **
**                         Neuroinformatics Group                  **
** Website: www.iclcv.org and                                      **
**          http://opensource.cit-ec.de/projects/icl               **
**                                                                 **
** File   : ICLMath/src/Homography2D.cpp                           **
** Module : ICLMath                                                **
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

#include <ICLMath/Homography2D.h>
#include <ICLMath/DynMatrix.h>

#include <ICLMath/FixedVector.h>

namespace icl{
  template<class T>
  GenericHomography2D<T>::GenericHomography2D(const Point32f *x, const Point32f *y, int n,GenericHomography2D<T>::Algorithm alg){
    if(alg == Simple){
      DynMatrix<T> X(n,3),Y(n,3);
      for(int i=0;i<n;++i){
        X(i,0) = x[i].x;
        X(i,1) = x[i].y;
        X(i,2) = 1;
        Y(i,0) = y[i].x; 
        Y(i,1) = y[i].y; 
        Y(i,2) = 1;
      }
      DynMatrix<T> H = X*Y.pinv(true); // A * B.pinv()
      
      std::copy(H.begin(),H.end(),Super::begin());
    }else{
      // actually we use the homography backwards
      std::swap(x,y);
      
      DynMatrix<T> M(8,2*n),r(1,2*n);
      for(int i=0;i<n;++i){
        T xx = x[i].x, xy=x[i].y, yx=y[i].x, yy=y[i].y;
        T *m = &M(0,2*i);
        m[0] = xx; 
        m[1] = xy;
        m[2] = 1;
        m[5] = m[4] = m[3] = 0;
        m[6] = - xx * yx;
        m[7] = - xy * yx;
        m+=8;
        m[2] = m[1] = m[0] = 0;
        m[3] = xx; 
        m[4] = xy;
        m[5] = 1;
        m[6] = - xx * yy;
        m[7] = - xy * yy;
        
        r[2*i] = yx;
        r[2*i+1] = yy;
      }


      
      DynMatrix<T> h  = M.solve(r,"svd");


      std::copy(h.begin(),h.end(),Super::begin());
      this->operator[](8) = 1;
    }
  }
  
  template struct GenericHomography2D<float>;
  template struct GenericHomography2D<double>;

}