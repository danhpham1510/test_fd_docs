import subprocess, os, sys

sys.path.insert(0, os.path.abspath('..'))  

subprocess.call('make clean', shell=True)
subprocess.call("cd ../../doxygen; doxygen", shell=True)




extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.doctest',
    'sphinx.ext.mathjax',
    'sphinx.ext.viewcode',
    'sphinx.ext.imgmath', 
    'sphinx.ext.todo',
    'breathe',
    'sphinxcontrib.seqdiag',
]

project = u'Aura Fall Detection'
copyright = u'2022, Select Technology'
version = '0.8'


xml_path = os.path.join("../../doxygen/build/", "xml")


breathe_projects = { "Fall":  xml_path}
breathe_default_project = "Fall"


templates_path = ['_templates']
html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
pygments_style = "sphinx"
