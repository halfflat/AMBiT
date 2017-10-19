import subprocess # Required to call ./getGitInfo.sh
import os # Required to get the current working dir
import shutil # Required for copying files
import ConfigParser

def get_build_target(src, module, build):
    # Constructs the path to the build object for some C++ source file, e.g.: 
    # Atom/ambit.cpp -> Atom/$BUILD/ambit.o

    # Strip the directory/module name and ".cpp" extension from the file
    basename = src[:src.find("cpp")].split('/')[1]
    obj_filename = basename + 'o'
    target = "{}/{}/{}".format(module, build, obj_filename)
    return(target)

### Custom configuration/autoconf-like functions for AMBiT ###
def check_pkgconfig(context):
    # Checks that pkg-config exists and is configured
    context.Message("Checking pkg-config version... ")
    ret = context.TryAction("pkg-config --version")[0]
    context.Result(ret)
    return(ret)

def check_pkg(context, lib):
    # Checks that the specified library exists (and is findable by pkg-config)
    context.Message("Attempting to automatically find {}... ".format(lib))
    ret = context.TryAction("pkg-config --exists {}".format(lib))[0]
    context.Result(ret)
    return(ret)

### Overlord configuration function ###
def configure_environment(env, conf, ambit_conf):
    """ Configures the supplied compilation environment according to the configuration options in conf.
        env - Pre-existing compilation environment, which should already have been initialised
        conf - Dict generated by parsing the configuration files (config.ini)
    """

    # Now grab the external libraries we need to link to, as well as their path and append to the compilation environment
    # The AMBiT directory will default to the current working directory if not specified in the config
    # file
    ambit_dir = conf.get("Build options", "AMBiT path")
    if not ambit_dir:
        ambit_dir = dir_path = os.getcwd()
        print("AMBiT path not specified. Defaulting to {}".format(ambit_dir))

    # NOTE: These have their own, specially defined paths because they're usually in lots of places 
    # and don't play nicely with pkg-config and/or SCons builtin configuration checks
    gtest_libs_dir = conf.get("Build options","gtest path")
    eigen_dir = conf.get("Build options", "Eigen path")
    sparsehash_dir = conf.get("Build options", "Sparsehash path")

    libs = [s.strip() for s in ambit_conf.get("Dependencies", "Libs").split(',')]
    lib_path = conf.get("Build options", "Lib path")
    
    # Angular data should go in the AMBiT directory if not specified in the config file
    angular_data_dir= conf.get("Build options", "Angular data")
    if not angular_data_dir:
        angular_data_dir = ambit_dir + "/AngularData"
        print("Angular data directory not specified. Defaulting to {}".format(ambit_dir))
    env.Append(CXXFLAGS = '-DANGULAR_DATA_DIRECTORY={}'.format(angular_data_dir))

    header_path =  [ambit_dir, eigen_dir , gtest_libs_dir , sparsehash_dir]

    env.Append(CPPPATH=header_path)
    env.Append(LIBPATH=lib_path)
    env.Append(LIBS=libs)

    # Also grab the user specified compiler and compiler flags. This section is optional, so the keys may not 
    # exist
    if conf.get("Build options", "CXXFLAGS"):
        custom_cxx_flags = conf.get("Build options", "CXXFLAGS")
        env.Append(CXXFLAGS = custom_cxx_flags)

        # Only replace the default compiler if the user has specified an alternative
    if conf.get("Build options", "CXX"):
        custom_cxx = conf.get("Build options", "CXX")
        if custom_cxx:
            env.Replace(CXX = custom_cxx)
    # Do the same for the Fortran compiler
    if conf.get("Build options", "F77"):
        custom_F77 = conf.get("Build options", "F77")
        if custom_F77:
            env.Replace(FORTRAN = custom_F77)
    # Now check everything works as expected (user options, custom compilers, etc.) and finish configuration
    env_conf = Configure(env, custom_tests = {"check_pkgconfig": check_pkgconfig, 
                                              "check_pkg": check_pkg})

    pkgconfig_exists = env_conf.check_pkgconfig()

    # Check the C++ and Fortran compilers exist and are properly configured
    if not env_conf.CheckCXX():
        print("Error: C++ compiler improperly installed/configured. Aborting")
        exit(-1)
    if not env_conf.CheckProg(env_conf.env['FORTRAN']):
        print("Error: Fortran compiler improperly installed/configured. Aborting.")
        exit(-1)

    # Run through the required libs, two approaches to find libraries:
    #   1) Look for it in the path specified in the configuration file
    #   2) Fallback: Try to find it automatically with pkg-config
    # If neither of these work then warn the user and bail out. Don't check for Boost here, since it doesn't support pkg-config
    for lib in [l for l in libs if l.find("boost") == -1]:
        if not env_conf.CheckLib(lib): # Can't find in current path
            if(not pkgconfig_exists or not env_conf.check_pkg(lib)):
                print("Warning: could not find library {}. Check Lib path in config.ini".format(lib))
                exit(-1)
            else:
               env_conf.env.ParseConfig("pkg-config --libs --cflags {}".format(lib))

    # Finally check for Boost, Eigen and Sparsehash and finish up configuration
    if not env_conf.CheckCXXHeader("boost/version.hpp"):
        print("Warning: could not find Boost headers. Check Lib path in config.ini")
        exit(-1)

    if not conf.get("Build options", "Sparsehash path"):
        print("Sparsehash directory not specified...")
        if env_conf.check_pkg("libsparsehash"):
            env_conf.env.ParseConfig("pkg-config --libs --cflags libsparsehash")
        else:
            print("Failed to automatically locate Sparsehash headers. Specify Sparsehash path in config.ini")
            exit(-1)
    if not conf.get("Build options", "Eigen path"):
        print("Eigen directory not specified...")
        if env_conf.check_pkg("eigen3"):
            env_conf.env.ParseConfig("pkg-config --libs --cflags eigen3")
        else:
            print("Failed to automatically locate Eigen headers. Specify Eigen path in config.ini")
            exit(-1)
            
    env = env_conf.Finish()
    return(env)



### Begin SCons commands ###

# Before we construct anything, make sure to update the gitinfo header file
with open("gitInfo.h", 'w') as fp:
    subprocess.call("./getGitInfo.sh", stdout=fp)

# First, grab the type of build from the command line and set up the compiler environment (default is gcc)
env = Environment(CXX = 'g++', CC = 'gcc', LINK = 'g++', \
    FORTRAN = 'gfortran', \
    F77FLAGS = '-O2')
if 'debug' in COMMAND_LINE_TARGETS:
    build = "Debug"
    env.Append(CXXFLAGS = '-std=c++11 -g -Wno-deprecated-register -Wno-unused-result -O0')
else:
    build = "Release"
    env.Append(CXXFLAGS = '-std=c++11 -O3')

# Now open the ini files containing the user config options, and the AMBiT requirements
conf = ConfigParser.SafeConfigParser(allow_no_value = True)
conf.optionxform = str # Necessary to make keys case-sensitive
if not conf.read("config.ini"):
    print("Warning: configuration file config.ini not found. Copying configuration template ...")
    shutil.copy2("config_template.ini", "config.ini")
    conf.read("config.ini")

ambit_conf = ConfigParser.SafeConfigParser(allow_no_value = True)
ambit_conf.optionxform = str # Necessary to make keys case-sensitive
# Bail out if we can't read config.ini
if not ambit_conf.read("ambit_dependencies.ini"):
    print("Error: could not read dependencies file ambit_dependencies.ini. Aborting build.")
    exit(-1)

# Configure the build environment, but only if we're not cleaning out targets
if not env.GetOption('clean'):
    env = configure_environment(env, conf, ambit_conf)

# And build all of our common libraries/modules
modules = ambit_conf.items("Modules")
common_libs = []
for module, files in modules:

    # First, split the comma separated list of files and strip out whitespace
    files = [s.strip() for s in files.split(',')]

    # Next, compile all the object files for our module

    srcs = [module + '/' + item for item in files]
    module_lib_target = module + '/' + build + '/' + module
    
    # Compile the object files
    objs = [env.Object(target = get_build_target(src, module, build), source = src) 
            for src in srcs]
    
    # Now link them all into one library
    common_libs.append(Library(target = module_lib_target, source = objs))

# Finally, put it all together in one executable (note that debug and release both alias to the ambit 
# executable. Eventually I'll add a test target in as well)
env.Program(target = 'ambit', source = common_libs)
env.Alias('release', 'ambit')
env.Alias('debug', 'ambit')
