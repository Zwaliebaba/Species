#ifndef INCLUDED_INPUTSPECLIST_H
#define INCLUDED_INPUTSPECLIST_H

#include <memory>

#include "InputSpec.h"
#include "AutoVector.h"


typedef auto_vector<const InputSpec> InputSpecList;

typedef InputSpecList::const_iterator InputSpecIt;

typedef std::unique_ptr<const InputSpec> InputSpecPtr;

typedef std::unique_ptr<const InputSpecList> InputSpecListPtr;


#endif // INCLUDED_INPUTSPECLIST_H
