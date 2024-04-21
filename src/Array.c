// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmArray_IMPLEMENT(DmBandList, DmBand, DmBand_free(itm));
DmArray_IMPLEMENT(DmPartList, DmPart, DmPart_free(itm));
DmArray_IMPLEMENT(DmPatternList, DmPattern, DmPattern_free(itm));
DmArray_IMPLEMENT(DmPartReferenceList, DmPartReference, DmPartReference_free(itm));
DmArray_IMPLEMENT(DmInstrumentList, DmInstrument, DmInstrument_free(itm));
DmArray_IMPLEMENT(DmResolverList, DmResolver, );
DmArray_IMPLEMENT(DmStyleCache, DmStyle*, DmStyle_release(*itm));
DmArray_IMPLEMENT(DmDlsCache, DmDls*, DmDls_release(*itm));
DmArray_IMPLEMENT(DmMessageList, DmMessage, DmMessage_free(itm));
