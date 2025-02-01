REM Give all arguments a more readable local variable name
Set compilerDir=%1
Set projectDir=%~2
Set imageDir=%~3
Set imageName=%~4
Set isDebug=%5

REM Local Variables
Set reset=[0m
Set cyan=[46m

if %isDebug%=="true" (
echo %cyan% NOTE: You have built in DEBUG mode, therefore a .hex file was not generated. If you would like to generate a .hex file, please build in production mode. For more information, please open MCC and click on the question mark symbol next to the 16-bit Bootloader Library under Resource Management. %reset%
exit 0
)
