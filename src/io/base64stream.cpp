/**
 * Base64-enabled input and output streams
 *
 * This class allows easy encoding and decoding
 * of Base64 data with a stream interface, hiding
 * the implementation from the user.
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *
 * Copyright (C) 2004 Inkscape.org
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "base64stream.h"



namespace Inkscape
{
namespace IO
{

//#########################################################################
//# B A S E 6 4    I N P U T    S T R E A M
//#########################################################################

static int base64decode[] =
{
/*00*/    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
/*08*/    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
/*10*/    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
/*18*/    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
/*20*/    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
/*28*/    -1,   -1,   -1,   62,   -1,   -1,   -1,   63,
/*30*/    52,   53,   54,   55,   56,   57,   58,   59,
/*38*/    60,   61,   -1,   -1,   -1,   -1,   -1,   -1,
/*40*/    -1,    0,    1,    2,    3,    4,    5,    6,
/*48*/     7,    8,    9,   10,   11,   12,   13,   14,
/*50*/    15,   16,   17,   18,   19,   20,   21,   22,
/*58*/    23,   24,   25,   -1,   -1,   -1,   -1,   -1,
/*60*/    -1,   26,   27,   28,   29,   30,   31,   32,
/*68*/    33,   34,   35,   36,   37,   38,   39,   40,
/*70*/    41,   42,   43,   44,   45,   46,   47,   48,
/*78*/    49,   50,   51,   -1,   -1,   -1,   -1,   -1
};


/**
 *
 */ 
Base64InputStream::Base64InputStream(InputStream &sourceStream)
                    : BasicInputStream(sourceStream)
{
    outCount = 0;
}

/**
 *
 */ 
Base64InputStream::~Base64InputStream()
{
    close();
}

/**
 * Returns the number of bytes that can be read (or skipped over) from
 * this input stream without blocking by the next caller of a method for
 * this input stream.
 */ 
int Base64InputStream::available()
{
    if (closed )
        return 0;
    int len = source.available() * 2 / 3;
    return len;
}

    
/**
 *  Closes this input stream and releases any system resources
 *  associated with the stream.
 */ 
void Base64InputStream::close()
{
    if (closed)
        return;
    source.close();
    closed = true;
}
    
/**
 * Reads the next byte of data from the input stream.  -1 if EOF
 */ 
int Base64InputStream::get()
{
    if (closed)
        return -1;

    if (outCount > 0)
        {
        return outBytes[outCount--];
        }

    if (done)
        return -1;

    outCount = 0;

    while (outCount < 4)
        {
        int ch = source.get();
        if (ch < 0)
            {
            done = true;
            break;
            }
        if (ch <= 32 || ch > 126)
            {
            }
        else if (ch == '=')
            {
            }
        else
            {
            int byteVal = base64decode[ch & 0x7f];
            if (byteVal < 0)
                {
                //Bad lookup value
                }
            outBytes[outCount++] = byteVal;
            }
        }

    //now try again
    if (outCount > 0)
        {
        return outBytes[outCount--];
        }

    //none of the above
    return -1;
}


//#########################################################################
//# B A S E 6 4    O U T P U T    S T R E A M
//#########################################################################

static char *base64encode =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 *
 */ 
Base64OutputStream::Base64OutputStream(OutputStream &destinationStream)
                     : BasicOutputStream(destinationStream)
{
    column   = 0;
    outBuf   = 0L;
    bitCount = 0;
}

/**
 *
 */ 
Base64OutputStream::~Base64OutputStream()
{
    close();
}

/**
 * Closes this output stream and releases any system resources
 * associated with this stream.
 */ 
void Base64OutputStream::close()
{
    if (closed)
        return;

    //get any last bytes (1 or 2) out of the buffer
    if (bitCount == 16)
        {
        outBuf <<= 2;  //pad to make 18 bits

        int indx  = (int)((outBuf & 0x0003f000L) >> 12);
        int obyte = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x00000fc0L) >>  6);
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x0000003fL)      );
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        destination.put('=');
        }
    else if (bitCount == 8)
        {
        outBuf <<= 4; //pad to make 12 bits

        int indx  = (int)((outBuf & 0x00000fc0L) >>  6);
        int obyte = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x0000003fL)      );
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        destination.put('=');
        destination.put('=');
        }

    destination.put('\n');

    destination.close();
    closed = true;
}
    
/**
 *  Flushes this output stream and forces any buffered output
 *  bytes to be written out.
 */ 
void Base64OutputStream::flush()
{
    if (closed)
        return;
    //dont flush here.  do it on close()    
    destination.flush();
}



/**
 * Writes the specified byte to this output stream.
 */ 
void Base64OutputStream::put(int ch)
{
    if (closed)
        {
        //probably throw an exception here
        return;
        }

    outBuf   <<=  8;
    outBuf   !=  (ch & 0xff);
    bitCount +=  8;
    if (bitCount >= 24)
        {
        int indx  = (int)((outBuf & 0x00fc0000L) >> 18);
        int obyte = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x0003f000L) >> 12);
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x00000fc0L) >>  6);
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        indx      = (int)((outBuf & 0x0000003fL)      );
        obyte     = (int)base64encode[indx & 63];
        destination.put(obyte);

        bitCount = 0;
        outBuf   = 0L;
        if (++column >= 64)
            {
            destination.put('\n');
            column = 0;
            }
        }
}



} // namespace IO
} // namespace Inkscape


//#########################################################################
//# E N D    O F    F I L E
//#########################################################################
