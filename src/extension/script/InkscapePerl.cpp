/**
 * Perl Interpreter wrapper for Inkscape
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *
 * Copyright (C) 2004 Inkscape.org
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
 


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "InkscapePerl.h"

#include <stdio.h>

#include "inkscape_perl.pm.h"

/*
 * Generated by SWIG
 */
extern "C" int
InkscapePerlParseBuf(char *startupCodeBuf, char *codeBuf);

namespace Inkscape {
namespace Extension {
namespace Script {


/*
 *
 */
InkscapePerl::InkscapePerl()
{
}

    

/*
 *
 */
InkscapePerl::~InkscapePerl()
{

}

    
    


bool InkscapePerl::interpretScript(char *codeBuf)
{
    int ret = InkscapePerlParseBuf(inkscape_module_script, codeBuf);
    if (!ret)
        {
        return false;
        }
    return true;
}
    




}  // namespace Script
}  // namespace Extension
}  // namespace Inkscape

//#########################################################################
//# E N D    O F    F I L E
//#########################################################################
