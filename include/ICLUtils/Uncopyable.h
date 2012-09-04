/********************************************************************
**                Image Component Library (ICL)                    **
**                                                                 **
** Copyright (C) 2006-2012 CITEC, University of Bielefeld          **
**                         Neuroinformatics Group                  **
** Website: www.iclcv.org and                                      **
**          http://opensource.cit-ec.de/projects/icl               **
**                                                                 **
** File   : include/ICLUtils/Uncopyable.h                          **
** Module : ICLUtils                                               **
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

#pragma once

namespace icl{
  namespace utils{
    /// Class interface for un-copyable classes.
    /** In some cases, classes might not be copied e.g. if
        the implementation of this would be very complex and
        it provides no benefits to copy an instance of
        a particular class.\n
        To forbid, that instance of this class might be copied,
        you can either implement a private copy constructor and
        a private assignment operator, or you can inherit
        the Uncopyable class.\n
        The following example demonstrates how you can protect
        a picture from being copied:
        \code
        #include <ICLUtils/Uncopyable.h>
        
        class Picasso : public Uncopyable{
           ...
        }; 
        
        int main(){
          Picasso guernica;
          Picasso copyOfGuernica = guernica; // not allowed
          Picasso anotherTryToCopyGuernica( guernica ); // also not allowed
    
          return 0;
        }
        \endcode
    **/
    class Uncopyable{
      protected:
      /// Empty base constructor
      Uncopyable(){}
  
      private:
      /// forbidden copy constructor
      Uncopyable(const Uncopyable &other){
        (void)other;
      }
      /// forbidden assignment operator
      Uncopyable &operator=(const Uncopyable &other){
        (void)other;
        return *this;
      }
    };
  } // namespace utils
}

