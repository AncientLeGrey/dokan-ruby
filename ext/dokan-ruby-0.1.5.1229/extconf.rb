require 'mkmf'

$LDFLAGS="/DUNICODE /D_UNICODE /MD"

#dir_config('dokan', '..\\dokan', '..\\dokan\\objchk_wxp_x86\\i386')
dir_config('dokan', '.', '.')
have_header('dokan.h')
have_library('dokan')

create_makefile('dokan_lib')

