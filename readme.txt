Environment Variables needed for csurf compilation:

Beside the Reaper SDK the code also depends on the JUCE and BOOST libraries. So you will need to
indicate three SDK locations via environment variables.

Example: The juce SDK (v1.5 or above) is installed in C:\juce and the
header file is found in C:\juce\juce.h

Set the environment variable in 

Start -> MyComputer -> RightClick -> Properties -> Advanced ->
Environment Variables -> System Variables section -> New Button

Variable Name = JUCE
Variable Value = C:\juce

Of course your actual paths (Variable Values) may be different, but
the Variable Names must exactly agree with the ones above, those
Variable Names are the ones referenced in the Visual Studio project.

For the reaper extensions SDK repeat as above:

Example: C:\reaper_extension_sdk

Variable Name = REAPER_EXTENSION_SDK
Variable Value = C:\reaper_extension_sdk


And for the Boost Library (v 1.39.0 or above) repeat again as above:

Example: C:\boost

Variable Name = BOOST
Variable Value = C:\boost

Most parts of the Boost Library are header-only based, but
boost::signals needs to include it's corresponding lib. Please check
the boost homepage for instructions how to generate the libs.

