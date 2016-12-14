## The pykeyvi compilers

The compiler is also available from pykeyvi:

    import pykeyvi
    compiler = pykeyvi.KeyOnlyDictionaryCompiler()

    # repeat for every key
    compiler.Add("foo")
    
    # finally compile
    compiler.Compile()
    compiler.WriteToFile("/tmp/test.keyvi")

Other available Compiler in pykeyvi

type              | details
----------------- | --------------------------------------------------------------------------------------------- 
integer           | CompletionDictionaryCompiler
key-only          | KeyOnlyDictionaryCompiler
string            | StringDictionaryCompiler
json              | JsonDictionaryCompiler

For dictionaries with values, Add takes the value as second parameter:

    compiler.Add("foo", 42)
    
To ensure that you do not run out of disk space while compiling, set $TMPDIR to a disk with enough free space.
    
    export TMPDIR=/mnt/tmp

