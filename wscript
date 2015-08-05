# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
import os

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['default-compiler-flags'], tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs',
               'default-compiler-flags'])

    if not os.environ.has_key('PKG_CONFIG_PATH'):
        os.environ['PKG_CONFIG_PATH'] = ':'.join([
            '/usr/lib/pkgconfig',
            '/usr/local/lib/pkgconfig',
            '/opt/local/lib/pkgconfig'])
    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
                   uselib_store='NDN_CXX', mandatory=True)

def build(bld):
    bld.program(
        features='cxx',
        target='producer',
        source='src/producer/producer.cpp src/utils/CustomOptionDescription.cpp src/utils/OptionPrinter.cpp',
        use='NDN_CXX',
        )

    bld.program(
        features='cxx',
        target='consumer',
        source='src/consumer/consumer.cpp src/utils/CustomOptionDescription.cpp src/utils/OptionPrinter.cpp',
        use='NDN_CXX',
        )
