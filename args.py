import logging

import atexit
import cPickle
import os
import re
import string
import sys
import types
import readline   #allows editing of raw_input line as typed (so delete works :-)

class ArgDict (dict, logging.Logger):
  def __init__(self, filename = None, defaultParent = None):
    dict.__init__(self)
    self.filename      = filename
    atexit.register(self.save)
    self.interactive   = 1
    self.metadata      = {'help' : {}, 'default' : {}, 'parent' : {}, 'tester' : {}, 'dir' : {}, 'saveinparent' : {}}
    self.argRE         = re.compile(r'\$(\w+|\{[^}]*\})')
    self.defaultParent = self.resolveParent(defaultParent)
    return

  def resolveParent(self, parent):
    '''The parent databse can be specified as:
     - A filename, where $var and ${var} in the name will be expanded
     - A module name, so that the parent database will be <name>Arg.db in the module root
     - An ArgDict object
     The return value is an ArgDict object representing the parent
    '''
    if not parent is None:
      if isinstance(parent, str):
        parent = self.expandVars(parent)
        if not os.path.exists(parent) and sys.modules.has_key(parent):
          parent = os.path.join(os.path.dirname(sys.modules[parent].__file__), parent+'Arg.db')
        if not os.path.exists(parent):
          parent = None
        else:
          parent = ArgDict(parent)
      elif not isinstance(parent, ArgDict):
        parent = None
    return parent
#  If requested key is a directory try to use Filebrowser to get it
  def getDirectory(self,key,exist):
    try:
      import GUI.FileBrowser
      import SIDL.Loader
      db = GUI.FileBrowser.FileBrowser(SIDL.Loader.createClass('GUI.Default.DefaultFileBrowser'))
    except:
      return (0,None)
    if self.metadata['help'].has_key(key): db.setTitle(self.metadata['help'][key])
    else:                                  db.setTitle('Select the directory for'+key)
    db.setMustExist(exist)
    return (1,db.getDirectory())

  def __setitem__(self,key,value):
    if self.metadata['saveinparent'].has_key(key):
      p = self.getParent(key)
      p.data[key] = value
    else:
      self.data[key] = value
    
  def __getitem__(self, key):
    if dict.has_key(self, key): return self.data[key]
    (ok, item) = self.getMissingItem(key)
    if ok:
      dict.__setitem__.(self, key, item)
      return item
    else:
      return None

  def has_key(self, key):
    if dict.has_key(self, key):
      return 1
    elif self.getParent(key):
      return self.getParent(key).has_key(key)
    else:
      return 0

  def getMissingItem(self, key):
    if self.getParent(key):
      (ok, item) = self.retrieveItem(key, self.getParent(key))
      if ok: return (ok, item)
    return self.requestItem(key)

  def retrieveItem(self, key, parent):
    if parent.has_key(key):
      return (1, parent[key])
    else:
      return (0, None)

  def requestItem(self, key):
    if not self.interactive: return (0, None)
    if self.metadata['dir'].has_key(key): 
      (ok,value) = self.getDirectory(key,self.metadata['dir'][key])
      if ok: return (ok,value)
      
    if self.metadata['help'].has_key(key): print self.metadata['help'][key]
    while 1:
      try:
        value = self.parseArg(raw_input('Please enter value for '+key+':'))
      except KeyboardInterrupt:
        return (0, None)
      if self.metadata['tester'].has_key(key): 
        (result, value) = self.metadata['tester'][key].test(value)
        if result:
          break
        else:
          print 'Try again'
      else:
        break
    return (1, value)

  def save(self):
    self.debugPrint('Saving argument database in '+self.filename, 2, 'argDB')
    dbFile = open(self.filename, 'w')
    cPickle.dump(self, dbFile)
    dbFile.close()

  def inputDefaultArgs(self):
    for key in self.metadata['default'].keys():
      if not self.has_key(key): self[key] = self.metadata['default'][key]

  def inputEnvVars(self):
    for key in os.environ.keys():
      self[key] = self.parseArg(os.environ[key])

  def inputCommandLineArgs(self, argList):
    if not type(argList) == types.ListType: return
    for arg in argList:
      if not arg[0] == '-':
        if self.has_key('target') and not self['target'] == ['default']:
          self['target'].append(arg)
        else:
          self['target'] = [arg]
      else:
        # Could try just using eval() on val, but we would need to quote lots of stuff
        (key, val) = string.split(arg[1:], '=')
        self[key]  = self.parseArg(val)

  def input(self, clArgs = None):
    self.inputDefaultArgs()
    self.inputEnvVars()
    self.inputCommandLineArgs(clArgs)
    self.setFromArgs(self)
    if self.filename: self.debugPrint('Read source database from '+self.filename, 2, 'argDB')

  def setHelp(self, key, docString):
    self.metadata['help'][key] = docString

  def setDir(self, key,exist):
    self.metadata['dir'][key] = exist

  def setTester(self, key, docString):
    self.metadata['tester'][key] = docString

  def setDefault(self, key, default):
    self.metadata['default'][key] = default

  # if key is saved it is saved in parent database
  def setSaveInParent(self, key):
    self.metadata['saveinparent'][key] = 1

  def setParent(self, key, parent):
    '''Specify the parent for a specific key. Allowable parents are discussed in resolveParent()'''
    if not parent is None:
      db = self.resolveParent(parent)
      if db is None:
        raise RuntimeError('Invalid parent database ('+parent+') for '+key)
      parent = db
    self.metadata['parent'][key] = parent
    return

  def getParent(self, key):
    isdefault = 0
    if self.metadata['parent'].has_key(key):


      return self.metadata['parent'][key]
    elif self.defaultParent:
      return self.defaultParent
    return None

  def parseArg(self, arg):
    if arg and arg[0] == '[' and arg[-1] == ']':
      if len(arg) > 2:
        arg = string.split(arg[1:-1], ',')
      else:
        arg = []
    return arg

  def expandVars(self, path):
    """Expand arguments of form $var and ${var}"""
    if '$' not in path: return path
    i = 0
    while 1:
      m = self.argRE.search(path, i)
      if not m:
        break
      i, j = m.span(0)
      name = m.group(1)
      if name[:1] == '{' and name[-1:] == '}':
        name = name[1:-1]
      tail = path[j:]
      path = path[:i] + self[name]
      i    = len(path)
      path = path + tail
    return path
