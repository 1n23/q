/****************************************************************************/
/***                                                                      ***/
/***   (C) 2012-2014 Dierk Ohlerich et al., all rights reserved.          ***/
/***                                                                      ***/
/***   Released under BSD 2 clause license, see LICENSE.TXT               ***/
/***                                                                      ***/
/****************************************************************************/

#include "altona2/libs/base/base.hpp"

extern "C" char test_txt[];

void Altona2::Main()
{
  sPrint(test_txt);
}

/****************************************************************************/
