from building import *
import os

# Import environment variables
Import('env')

# Get the current working directory
cwd = GetCurrentDir()

# Initialize include paths and source files list
path = [os.path.join(cwd, 'Include')]
src = [os.path.join(cwd, 'Source', 'Templates', 'system_stm32f4xx.c')]

# Map microcontroller units (MCUs) to their corresponding startup files
mcu_startup_files = {
    'STM32F401xC': 'startup_stm32f401xc.s',
    'STM32F401xE': 'startup_stm32f401xe.s',
    'STM32F405xx': 'startup_stm32f405xx.s',
    'STM32F407xx': 'startup_stm32f407xx.s',
    'STM32F410Cx': 'startup_stm32f410cx.s',
    'STM32F410Rx': 'startup_stm32f410rx.s',
    'STM32F410Tx': 'startup_stm32f410tx.s',
    'STM32F411xE': 'startup_stm32f411xe.s',
    'STM32F412Cx': 'startup_stm32f412cx.s',
    'STM32F412Rx': 'startup_stm32f412rx.s',
    'STM32F412Vx': 'startup_stm32f412vx.s',
    'STM32F412Zx': 'startup_stm32f412zx.s',
    'STM32F413xx': 'startup_stm32f413xx.s',
    'STM32F415xx': 'startup_stm32f415xx.s',
    'STM32F417xx': 'startup_stm32f417xx.s',
    'STM32F423xx': 'startup_stm32f423xx.s',
    'STM32F427xx': 'startup_stm32f427xx.s',
    'STM32F429xx': 'startup_stm32f429xx.s',
    'STM32F437xx': 'startup_stm32f437xx.s',
    'STM32F439xx': 'startup_stm32f439xx.s',
    'STM32F446xx': 'startup_stm32f446xx.s',
    'STM32F469xx': 'startup_stm32f469xx.s',
    'STM32F479xx': 'startup_stm32f479xx.s',
}

# Check each defined MCU, match the platform and append the appropriate startup file
cpp_defines_tuple = env.get('CPPDEFINES', [])
cpp_defines_list = [item[0] if isinstance(item, tuple) else item for item in cpp_defines_tuple]
for mcu, startup_file in mcu_startup_files.items():
    if mcu in cpp_defines_list:
        if rtconfig.PLATFORM in ['gcc', 'llvm-arm']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'gcc', startup_file)]
        elif rtconfig.PLATFORM in ['armcc', 'armclang']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'arm', startup_file)]
        elif rtconfig.PLATFORM in ['iccarm']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'iar', startup_file)]
        break

# Define the build group
group = DefineGroup('STM32F4-CMSIS', src, depend=['PKG_USING_STM32F4_CMSIS_DRIVER'], CPPPATH=path)

# Return the build group
Return('group')
