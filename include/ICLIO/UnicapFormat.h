#ifndef ICL_UNICAP_FORMAT_H
#define ICL_UNICAP_FORMAT_H

#include <unicap.h>
#include <ICLUtils/Rect.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <ICLUtils/SmartPtr.h>


namespace icl{
  /// Wrapper class for the unicap_format_t struct \ingroup UNICAP_G
  /** The UnicapFormat class wraps the unicap_format_t struct and provides accessibility to
      all of its members by setter and getter functions. Additionally it uses a Smart-Pointer
      to allow cheep (shallow) copies by using the assign operator \n
      <b>Note: When using the UnicapFormats assign-operator or copy constructor, the object
      is copied in a shallow way! </b> \n
      A UnicapFormat provides the following data elements:
      - ID [string]
      - Size [available as Rect (unicap-flavor) and Size (icl-flavor)]
      - minimum size [as above]
      - maximum size [as above]
      - horizontal size stepping [int]
      - vertical size stepping [int]
      - a list of all possible sizes [vector<Rects or Sizes>]
      - bits per pixel [int]
      - a 4-chars format description (fourcc) [string]
      - flags [uint]
      - allowed buffer-types (??: 1:system and user, 0:only user) [int]
      - system buffer count [int]
      - size of a single image data buffer in bytes [int]
      - current buffer type [UnicapFormat::buffertype]
      - access to the underlying unicap_format_t (const and not const)
      - access to the underlying unicap_handle_t (const and not const)
      
      Furthermore, the following high-level utility functions are provided:
      - checkSize ( checks whether a size is supported by this format or not )
      - toString ( creates a string explanation of this format )
  */
  class UnicapFormat{
    public:
    
    /// Create an empty UnicapFormat
    UnicapFormat();
    
    /// Create a new UnicapFormat with given unicap-handle
    UnicapFormat(unicap_handle_t handle);

    /// wrapper of the unicap-buffer type (?? system -> DMA ??)
    enum buffertype{
      userBuffer = UNICAP_BUFFER_TYPE_USER, /**< using user buffer (data in user space) */
      systemBuffer = UNICAP_BUFFER_TYPE_SYSTEM /**< using system buffer (data in system space) */
    };

    /// returns an identifier string of this format
    std::string getID() const;

    /// returns the size of this format (as Rect)
    Rect getRect() const;
    
    /// returns the size of this format
    Size getSize() const;

    /// returns the minimum size of this format (as Rect)
    Rect getMinRect() const;

    /// returns the minimum size of this format     
    Rect getMaxRect() const;
    
    /// returns the maximum size of this format (as Rect)
    Size getMinSize() const;

    /// returns the maximum size of this format
    Size getMaxSize() const;

    /// returns the horizontal stepping of this format
    int getHStepping() const;
    
    /// returns the vertical stepping of this format
    int getVStepping() const;

    /// returns all possible sizes of this format (as Rects)
    std::vector<Rect> getPossibleRects() const;

    /// returns all possible sizes of this format
    std::vector<Size> getPossibleSizes() const;

    /// returns the number of bits of each image pixel of this format
    int getBitsPerPixel() const;
    
    /// returns a 4-char format description of this format
    std::string getFourCC() const;

    /// returns additional flags or this format
    unsigned int getFlags() const;
    
    /// returns the allowed buffer types for this format
    /** Probably: 0 = user-buffers only, 1=user and system buffers allowed ?*/
    unsigned int getBufferTypes() const;
    
    /// returns the count of supported system buffers for this format
    unsigned int getSystemBufferCount() const;
    
    /// returns the byte count of an image buffer for this format
    unsigned int getBufferSize() const;
    
    /// returns the current buffer type of this format (one of system or user)
    buffertype getBufferType() const;

    /// returns a pointer to the underlying unicap_format_t struct (const)
    const unicap_format_t *getUnicapFormat() const;

    /// returns a pointer to the underlying unicap_format_t struct 
    unicap_format_t *getUnicapFormat();

    /// returns a pointer to the underlying unicap_handle_t (const)
    const unicap_handle_t getUnicapHandle() const;

    /// returns a pointer to the underlying unicap_handle_t 
    unicap_handle_t getUnicapHandle();
    
    /// returns a string explanation of this format
    std::string toString()const;
    
    /// checks whether this format does support the given size or not
    bool checkSize(const Size &size)const;

    private:
    /** \cond **/
    struct UnicapFormatDelOp : public DelOpBase{ 
      static void delete_func(unicap_format_t *f){
        // it seems like that other variables are stored elsewhere
        //if(f->size_count && f->sizes) free (f->sizes);
        free(f);
      }
    };
    /** \endcond **/

    /// internal storage for the wrapped unicap_format_t struct (SmartPtr)
    SmartPtrBase<unicap_format_t, UnicapFormatDelOp> m_oUnicapFormatPtr;
    
    /// internal storage for the underlying unicap_handle_t
    unicap_handle_t m_oUnicapHandle;
  };

  
} // end of namespace icl

#endif