#!/usr/bin/env python

import os
from setuptools import setup
os.listdir

setup(
   name='sse-t8264',
   version='1.0',
   description='Module to initialize Supermicro T8264 platforms',

   packages=['sse-t8264'],
   package_dir={'sse-t8264': ''}
)


