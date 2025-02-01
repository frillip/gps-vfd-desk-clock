# Give all arguments a more readable local variable name
export compilerDir="$1"
export projectDir="$2"
export imageDir="$3"
export imageName="$4"
export isDebug="$5"

# Local Variables
export reset="[0m"
export cyan="[46m"

if [ $isDebug = "true" ]; then
echo -e "$cyan NOTE: You have built in DEBUG mode, therefore a .hex file was not generated. If you would like to generate a .hex file, please build in production mode. For more information, please open MCC and click on the question mark symbol next to the 16-bit Bootloader Library under Resource Management. $reset"
exit 0
fi
