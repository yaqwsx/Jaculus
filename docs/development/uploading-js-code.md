# Uploading JS code

Currently, there is a primitive tool in `tools/transfer.py` that allows you to
manipulate with the internal memory for Javascript programs. If you ever
struggle, just use `tools/transfer.py --help`.

## Uploading a program

To upload a program invoke:

```
tools/transfer.py sync --dir directoryWithTheProgram
```

The tool should finish. Note that if you have concurrently opened `idf.py
monitor` the procedure fails.

## Transpiling programs

If you would like to test the programs that use the `await` and `async`
keywords, you have to transpile the program. This is best done using
[regenerator](https://github.com/facebook/regenerator). Usually you will invoke
it as `npx regenerator sourceDirectory buildDirectory`. I will skip the
installation procedure as soon it will not be needed and the power users don't
need instructions :-P.
