#!/bin/python

from __future__ import print_function

try:
    from builtins import range
except:
    pass
try:
    from builtins import object
except:
    pass

autogen_warn = "// This file is autogenerated - see gen_fieldops.py"
print(autogen_warn)
print("""#include <field3d.hxx>
#include <field2d.hxx>
#include <bout/mesh.hxx>
#include <globals.hxx>
#include <interpolation.hxx>
""")
# A class to keep all the data of the different fields
class Field(object):
    # name
    n=''
    # directions
    d=[]
    # identifier - short string
    i=''
    def __init__(self,name,dirs,idn):
        self.n=name
        self.d=dirs
        self.i=idn
        # self.a : how to pass data
        if idn=='real':
            self.a=name
        else:
            self.a='const %s &'%name
    # how to get value from field
    def get(self,name):
        if self.i=='real':
            return name
        elif self.i=='f2d':
            return '%s[y+x*ny]'%name
        else:
            return '%s[z+nz*(y+ny*x)]'%(name)

# Declare what fields we currently support:
# Field perp is currently missing
f3d =Field('Field3D' ,['x','y','z'],'f3d')
f2d =Field('Field2D' ,['x','y'    ],'f2d')
real=Field('BoutReal',[           ],'real')
fields=[f3d,f2d,real]
import sys
def mymax(f1,f2):
    if f1.i==f2.i:
        return f1
    elif f1.i == 'real':
        return f2
    elif f2.i == 'real':
        return f1
    else:
        return f3d
def mymin(f1,f2):
    if (len(f1.d) < len(f2.d)):
        return f1
    else:
        return f2

# declare the possible operators + names
ops=['*','/','+','-']
op_names={'*':'mul',
          '/':'div',
          '-':'minus',
          '+':'plus'}

# loop over all fields for lhs and rhs of the operation. Generates the
# not-in-place variants of the operations, returning a new field.
for lhs in fields:
    for rhs in fields:
        if lhs.i == rhs.i == 'real': # we don't have define real real operations
            continue
        # if both fields are the same, or one of them is real, we
        # don't need to care what element is stored where, but can
        # just loop directly over everything, using a simple c-style
        # for loop. Otherwise we need x,y,z of the fields.
        if (lhs != rhs and mymin(lhs,rhs).i != 'real'):
            elementwise=True
        else:
            elementwise=False
        # the output of the operation. The `larger` of the two fields.
        out=mymax(rhs,lhs)
        for op in ops:
            opn=op_names[op]
            # *************************************************************
            # start of the function header - doing the operation
            print(autogen_warn)
            print("// Do the actual %s of %s and %s"%(opn,lhs.n,rhs.n))
            print('void autogen_%s_%s_%s_%s('%(out.n,lhs.n,rhs.n,opn), end=' ')
            # the first element (intent out) shouldn't be const, the
            # rest should be (intent in)
            const=''
            fs=[out,lhs,rhs]
            fn=['result','lhs','rhs']
            for f in range(len(fs)):
                print(const,"BoutReal", end=' ')
                if fs[f].i != 'real':
                    # use gcc extension to tell the compiler that this
                    # array is not aliased. Cleaner would be to put
                    # this in a different file and compile with a C
                    # compiler, as this is supported by the C99
                    # standard, but not by C++
                    print("* __restrict__", end=' ')
                print(fn[f],",", end=' ')
                const='const'
            # depending on how we loop over the fields, we need to now
            # x,y and z, or just the total number of elements
            if elementwise:
                c=''
                for d in out.d:
                    print('%s int n%s'%(c,d), end=' ')
                    c=','
            else:
                print(' int max', end=' ')
            print('){')
            # end of function header.
            # *************************************************************
            # start of function:
            if elementwise:
                # we need to loop over all dimension of the out file
                for d in out.d:
                    print('  for (int %s=0;%s<n%s;++%s){'%(d,d,d,d))
                print("      ",out.get('result'),"=", end=' ')
                print(lhs.get('lhs'), end=' ')
                print(op, end=' ')
                print(rhs.get('rhs'), end=' ')
                print(";")
                for d in out.d:
                    print("  }")
            else:
                print("  for (int i=0;i<max;++i){")
                print("    result[i]=lhs", end=' ')
                if lhs.i != 'real':
                    print("[i]", end=' ')
                print("%srhs"%op, end=' ')
                if rhs.i != 'real':
                    print("[i]", end=' ')
                print(";")
                print("  }")
            print("}")
            # end of function
            # *************************************************************
            # beginning of the function, taking the fields - the C++
            # function. This does no actual computation.
            print(autogen_warn)
            print("// Provide the C++ wrapper for %s of %s and %s"%(opn,lhs.n,rhs.n))
            print("%s operator%s(%s lhs,%s rhs){"%(out.n,op,lhs.a,rhs.a))
            print("  Indices i{0,0,0};")
            print("  %s result;"%(out.n))
            print("  result.allocate();")
            print("  checkData(lhs);")
            print("  checkData(rhs);")
            # call the C function to do the work.
            print("  autogen_%s_%s_%s_%s("%(out.n,lhs.n,rhs.n,opn), end=' ')
            for f in range(len(fn)):
                if fs[f].i=='real':
                    print("%s,"%fn[f], end=' ')
                else:
                    print("&%s[i],"%fn[f], end=' ')
            m=''
            print('\n             ', end=' ')
            for d in out.d:
                print(m,"mesh->LocalN%s"%d, end=' ')
                if elementwise:
                    m=','
                else:
                    m='*'
            print(");")
            # hardcode to only check field location for Field 3D
            if lhs.i == rhs.i == 'f3d':
                print("#ifdef CHECK")
                print("  if (lhs.getLocation() != rhs.getLocation()){")
                print('    throw BoutException("Trying to %s fields of different locations. lhs is at %%s, rhs is at %%s!",strLocation(lhs.getLocation()),strLocation(rhs.getLocation()));'%op_names[op])
                print('  }')
                print('#endif')
            # Set out location (again, only for f3d)
            if out.i == 'f3d':
                if lhs == 'f3d':
                    src='lhs'
                elif rhs.i == 'f3d':
                    src='rhs'
                elif lhs.i != 'real':
                    src='lhs'
                else:
                    src='rhs'
                print("  result.setLocation(%s.getLocation());"%src)
            # Check result and return
            print("  checkData(result);")
            print("  return result;")
            print("}")
            print()
            print()


# generate the operators for updating the lhs in place
for lhs in fields:
    for rhs in fields:
        # no real real operation
        if lhs.i == rhs.i == 'real':
            continue
        if (lhs != rhs and mymin(lhs,rhs).i != 'real'):
            elementwise=True
        else:
            elementwise=False
        out=mymax(rhs,lhs)
        if out == lhs:
            for op in ops:
                opn=op_names[op]
                # *********************************************************
                # start of the function header - doing the operation
                print(autogen_warn)
                print("// Provide the C function to update %s by %s with %s"%(lhs.n,opn,rhs.n))
                print('void autogen_%s_%s_%s('%(lhs.n,rhs.n,opn), end=' ')
                const=''
                fs=[lhs,rhs]
                fn=['lhs','rhs']
                for f in range(len(fs)):
                    print(const,"BoutReal", end=' ')
                    if fs[f].i != 'real':
                        print("* __restrict__", end=' ')
                    print(fn[f],",", end=' ')
                    const='const'
                if elementwise:
                    c=''
                    for d in out.d:
                        print('%s int n%s'%(c,d), end=' ')
                        c=','
                else:
                    print(' int max', end=' ')
                print('){')
                # end of function header
                # *********************************************************
                # beginning of function
                if elementwise:
                    for d in out.d:
                        print('  for (int %s=0;%s<n%s;++%s){'%(d,d,d,d))
                    print("      ",out.get('lhs'),"%s="%op, end=' ')
                    print(rhs.get('rhs'), end=' ')
                    print(";")
                    for d in out.d:
                        print("  }")
                else:
                    print("  for (int i=0;i<max;++i){")
                    print("    lhs[i]%s=rhs"%op, end=' ')
                    if rhs.i != 'real':
                        print("[i]", end=' ')
                    print(";")
                    print("  }")
                print("}")
                # end of function
                # *********************************************************
                # beginning of C++ function
                print(autogen_warn)
                print("// Provide the C++ operator to update %s by %s with %s"%(lhs.n,opn,rhs.n))
                print("%s & %s::operator %s="%(lhs.n,lhs.n,op), end=' ')
                print("(%s rhs){"%(rhs.a))
                print("  // only if data is unique we update the field")
                print("  // otherwise just call the non-inplace version")
                print("  if (data.unique()){")
                print("    Indices i{0,0,0};")
                print("    checkData(*this);")
                print("    checkData(rhs);")
                print("    autogen_%s_%s_%s(&(*this)[i],"%(lhs.n,rhs.n,opn), end=' ')
                fs=[rhs]
                fn=['rhs']
                for f in range(len(fn)):
                    if fs[f].i=='real':
                        print("%s,"%fn[f], end=' ')
                    else:
                        print("&%s[i],"%fn[f], end=' ')
                m=''
                print('\n             ', end=' ')
                for d in out.d:
                    print(m,"mesh->LocalN%s"%d, end=' ')
                    if elementwise:
                        m=','
                    else:
                        m='*'
                print(");")
                # if both are f3d, make sure they are in the same location
                if lhs.i == rhs.i == 'f3d':
                    print("#ifdef CHECK")
                    print("  if (this->getLocation() != rhs.getLocation()){")
                    print('    throw BoutException("Trying to %s fields of different locations!");'%op_names[op])
                    print('  }')
                    print('#endif')
                print("    checkData(*this);")
                print("  } else {")
                print("    (*this)= (*this) %s rhs;"%op)
                print("  }")
                print("  return *this;")
                print("}")
                print()
                print()
                
