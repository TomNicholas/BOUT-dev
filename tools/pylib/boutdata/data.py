# Provides a class BoutData which makes access to code
# inputs and outputs easier. Creates a tree of maps,
# inspired by approach used in OMFIT
#
# 

import os
import glob

from boutdata import collect

try:
    from boututils.datafile import DataFile
except ImportError:
    print("ERROR: boututils.datafile.DataFile couldn't be loaded")
    raise
    
class BoutOptions(object):
    """
    This class represents a tree structure.
    Each node (BoutOptions object) can have several
    sub-nodes (sections), and several key-value pairs.

    Example
    -------
    
    optRoot = BoutOptions()  # Create a root
    
    # Specify value of a key in a section "test" 
    # If the section does not exist then it is created
    
    optRoot.getSection("test")["key"] = value
    
    # Get the value of a key in a section "test"
    # If the section does not exist then a KeyError is raised
    
    print optRoot["test"]["key"]
    
    # To pretty print the options
    
    print optRoot
    
    """
    def __init__(self, name="root", parent=None):
        self._sections = {}
        self._keys = {}
        self._name = name
        self._parent = parent

    def getSection(self, name):
        """
        Return a section object. If the section
        does not exist then it is created
        """
        name = name.lower()
        
        if name in self._sections:
            return self._sections[name]
        else:
            newsection = BoutOptions(name, self)
            self._sections[name] = newsection
            return newsection
        
    def __getitem__(self, key):
        """
        First check if it's a section, then a value
        """
        key = key.lower()
        if key in self._sections:
            return self._sections[key]
            
        if key not in self._keys:
            raise KeyError("Key '%s' not in section '%s'" % (key, self.path()))
        return self._keys[key]

    def __setitem__(self, key, value):
        """
        Set a key
        """
        if len(key) == 0:
            return
        self._keys[key.lower()] = value

    def path(self):
        """
        Returns the path of this section,
        joining together names of parents
        """
        
        if self._parent:
            return self._parent.path() + ":" + self._name
        return self._name

    def keys(self):
        """
        Returns all keys, including sections and values
        """
        return self._sections.keys() + self._keys.keys()

    def sections(self):
        """
        Return a list of sub-sections
        """
        return self._sections.keys()

    def values(self):
        """
        Return a list of values
        """
        return self._keys.keys()

    def __len__(self):
        return len(self._sections) + len(self._keys)

    def __iter__(self):
        """
        Iterates over all keys. First values, then sections
        """
        for k in self._keys:
            yield k
        for s in self._sections:
            yield s

    def __str__(self, indent=""):
        """
        Print a pretty version of the options tree
        """
        text = self._name + "\n"
        
        for k in self._keys:
            text += indent + " |- " + k + " = " + str(self._keys[k]) + "\n"
        
        for s in self._sections:
            text += indent + " |- " + self._sections[s].__str__(indent+" |  ")
        return text

class BoutOptionsFile(BoutOptions):
    """
    Parses a BOUT.inp configuration file, producing
    a tree of BoutOptions.
    
    Slight differences from ConfigParser, including allowing
    values before the first section header.

    Example
    -------
    
    opts = BoutOptionsFile("BOUT.inp")
    
    print opts   # Print all options in a tree
    
    opts["All"]["scale"] # Value "scale" in section "All"
    
    """
    def __init__(self, filename, name="root"):
        BoutOptions.__init__(self, name)
        # Open the file
        with open(filename, "r") as f:
            # Go through each line in the file
            section = self # Start with root section
            for linenr, line in enumerate(f.readlines()):
                # First remove comments, either # or ;
                startpos = line.find("#")
                if startpos != -1:
                    line = line[:startpos]
                startpos = line.find(";")
                if startpos != -1:
                    line = line[:startpos]
                
                # Check section headers
                startpos = line.find("[")
                endpos = line.find("]")
                if startpos != -1:
                    # A section heading
                    if endpos == -1:
                        raise SyntaxError("Missing ']' on line %d" % (linenr,))
                    line = line[(startpos+1):endpos].strip()
                    
                    section = self
                    while True:
                        scorepos = line.find(":")
                        if scorepos == -1:
                            break
                        sectionname = line[0:scorepos]
                        line = line[(scorepos+1):]
                        section = section.getSection(sectionname)
                    section = section.getSection(line)
                else:
                    # A key=value pair
                    
                    eqpos = line.find("=")
                    if eqpos == -1:
                        # No '=', so just set to true
                        section[line.strip()] = True
                    else:
                        value = line[(eqpos+1):].strip()
                        try:
                            # Try to convert to an integer
                            value = int(value)
                        except ValueError:
                            try:
                                # Try to convert to float
                                value = float(value)
                            except ValueError:
                                # Leave as a string
                                pass
                        
                        section[line[:eqpos].strip()] = value
                    
                        
            
class BoutOutputs(object):
    """
    Emulates a map class, represents the contents of a BOUT++ dmp files. Does
    not allow writing, only reading of data.  By default there is no cache, so
    each time a variable is read it is collected; if caching is set to True
    variables are stored once they are read.  Extra keyword arguments are
    passed through to collect.
    
    Example
    -------

    d = BoutOutputs(".")  # Current directory
    
    d.keys()     # List all valid keys

    d.dimensions["ne"] # Get the dimensions of the field ne
    
    d["ne"] # Read "ne" from data files

    d = BoutOutputs(".", prefix="BOUT.dmp", caching=True) # Turn on caching

    Options
    -------
    prefix - sets the prefix for data files (default "BOUT.dmp")

    caching - switches on caching of data, so it is only read into memory when
              first accessed (default False) If caching is set to a number, it
              gives the maximum size of the cache in GB, after which entries
              will be discarded in first-in-first-out order to prevent the
              cache getting too big.  If the variable being returned is bigger
              than the maximum cache size, then the variable will be returned
              without being added to the cache, and the rest of the cache will
              be left.

    **kwargs - keyword arguments that are passed through to collect()
    """
    def __init__(self, path=".", prefix="BOUT.dmp", caching=False, **kwargs):
        """
        Initialise BoutOutputs object
        """
        self._path = path
        self._prefix = prefix
        self._caching = caching
        self._kwargs = kwargs
        
        # Label for this data
        self.label = path

        # Check that the path contains some data
        file_list = glob.glob(os.path.join(path, prefix+"*.nc"))
        if len(file_list) == 0:
            raise ValueError("ERROR: No data files found")
        
        # Available variables
        self.varNames = []
        self.dimensions = {}
        self.evolvingVariableNames = []

        # Private variables
        if self._caching:
            from collections import OrderedDict
            self._datacache = OrderedDict()
            if self._caching is not True:
                # Track the size of _datacache and limit it to a maximum of _caching
                try:
                    # Check that _caching is a number of some sort
                    float(self._caching)
                except ValueError:
                    raise ValueError("BoutOutputs: Invalid value for caching argument. Caching should be either a number (giving the maximum size of the cache in GB), True for unlimited size or False for no caching.")
                self._datacachesize = 0
                self._datacachemaxsize = self._caching*1.e9
        
        with DataFile(file_list[0]) as f:
            # Get variable names
            self.varNames = f.keys()
            for name in f.keys():
                dimensions = f.dimensions(name)
                self.dimensions[name] = dimensions
                if name != "t_array" and "t" in dimensions:
                    self.evolvingVariableNames.append(name)
        
    def keys(self):
        """
        Return a list of available variable names
        """
        return self.varNames

    def evolvingVariables(self):
        """
        Return a list of names of time-evolving variables
        """
        return self.evolvingVariableNames
        
    def __len__(self):
        return len(self.varNames)
            
    def __getitem__(self, name):
        """
        Reads a variable using collect.
        Caches result and returns later if called again, if self._caching=True
        
        """
        if self._caching:
            if name not in self._datacache.keys():
                item = collect(name, path=self._path, prefix=self._prefix, **self._kwargs)
                if self._caching is not True:
                    itemsize = item.nbytes
                    if itemsize>self._datacachemaxsize:
                        return item
                    self._datacache[name] = item
                    self._datacachesize += itemsize
                    while self._datacachesize > self._datacachemaxsize:
                        self._removeFirstFromCache()
                else:
                    self._datacache[name] = item
                return item
            else:
                return self._datacache[name]
        else:
            # Collect the data from the repository
            data = collect(name, path=self._path, prefix=self._prefix, **self._kwargs)
            return data
    
    def _removeFirstFromCache(self):
        # pop the first item from the OrderedDict _datacache
        item = self._datacache.popitem(last=False)
        self._datacachesize -= item[1].nbytes

    def __iter__(self):
        """
        Iterate through all keys, starting with "options"
        then going through all variables for collect
        """
        for k in self.varNames:
            yield k
            
    def __str__(self, indent=""):
        """
        Print a pretty version of the tree
        """
        text = ""
        for k in self.varNames:
            text += indent+k+"\n"
        
        return text
    

def BoutData(path=".", prefix="BOUT.dmp", caching=False, **kwargs):
    """
    Returns a dictionary, containing the contents of a BOUT++ output directory.
    Does not allow writing, only reading of data.  By default there is no
    cache, so each time a variable is read it is collected; if caching is set
    to True variables are stored once they are read.
    
    Example
    -------

    d = BoutData(".")  # Current directory
    
    d.keys()     # List all valid keys

    print d["options"]  # Prints tree of options

    d["options"]["nout"]   # Value of nout in BOUT.inp file
    
    print d["outputs"]    # Print available outputs

    d["outputs"]["ne"] # Read "ne" from data files
    
    d = BoutData(".", prefix="BOUT.dmp", caching=True) # Turn on caching

    Options
    -------
    prefix - sets the prefix for data files (default "BOUT.dmp")

    caching - switches on caching of data, so it is only read into memory when
              first accessed (default False) If caching is set to a number, it
              gives the maximum size of the cache in GB, after which entries
              will be discarded in first-in-first-out order to prevent the
              cache getting too big.  If the variable being returned is bigger
              than the maximum cache size, then the variable will be returned
              without being added to the cache, and the rest of the cache will
              be left.
    
    **kwargs - keyword arguments that are passed through to collect()
    """
    
    data = {} # Map for the result
    
    data["path"] = path
    
    # Options from BOUT.inp file
    data["options"] = BoutOptionsFile(os.path.join(path, "BOUT.inp"), name="options")
    
    # Output from .dmp.* files
    data["outputs"] = BoutOutputs(path, prefix=prefix, caching=caching, **kwargs)
    
    return data
