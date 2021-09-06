#!/usr/bin/env python3

import click
import serial
import serial.tools.list_ports
import base64
import time
from dataclasses import dataclass
from enum import Enum
import os

class FileType(Enum):
    File = 1
    Directory = 2

@dataclass
class FsEntry:
    name: str
    type: FileType

def getPortPath(userSpec):
    if userSpec is not None:
        return userSpec
    ports = serial.tools.list_ports.comports()
    if len(ports) == 0:
        raise RuntimeError("No device connected")
    if len(ports) > 1:
        raise RuntimeError("Multiple devices available, please choose one")
    return ports[0].device

def acceptsSerialPort(function):
    function = click.option("-p", "--port", type=str, default=None,
        help="Serial port")(function)
    function = click.option("-b", "--baudrate", type=int, default=921600,
        help="Baudrate")(function)
    return function

def jumpIntoUploader(port):
    port.rts = False
    time.sleep(0.5) # Windows does not make the change immediately
    port.rts = True

def clearPort(port):
    t = port.timeout
    port.timeout = 0.5
    port.read(64 * 1024 * 1024)
    port.timeout = t

def exitUploader(port):
    port.write("EXIT\n".encode("utf-8"))
    return port.readline()

def listTargetEntries(port):
    port.write("LIST\n".encode("utf-8"))
    res = []
    while True:
        l = port.readline().decode("utf-8").strip()
        if len(l) == 0:
            break
        l = l.split()
        type = {
            "F": FileType.File,
            "D": FileType.Directory
        }[l[0]]
        res.append(FsEntry(l[1], type))
    return res

def delete(port, entry):
    port.write(f"REMOVE {entry}\n".encode("utf-8"))
    print(port.readline())

def isHiddenFile(path):
    """
    Given a path, check if it contains a hidden directory
    """
    path = os.path.normpath(path)
    return any([x[0] == "." and x != "." and x != ".." for x in path.split(os.sep)])

@click.command()
@acceptsSerialPort
@click.option("-d", "--dir", type=click.Path(file_okay=False, dir_okay=True, exists=True), default=None)
def sync(port, baudrate, dir):
    toUpload = []
    for root, dirs, files in os.walk(dir):
        files = [f for f in files if not isHiddenFile(os.path.join(root, f))]
        for f in files:
            with open(os.path.join(root, f), "rb") as file:
                content = base64.b64encode(file.read()).decode("utf-8")
            name = os.path.join(root, f)
            name = os.path.relpath(name, dir)
            toUpload.append((name, content))
    with serial.Serial(getPortPath(port), baudrate) as s:
        # Windows restarts ESP32, so there will be bootloader message
        time.sleep(1)
        clearPort(s)
        jumpIntoUploader(s)
        for entry in listTargetEntries(s):
            delete(s, entry.name)
        for f, content in toUpload:
            message = f"PUSH {f} {content}\n".encode("utf-8")
            print(message)
            CHUNK_SIZE = 256
            for chunk in [message[i:i + CHUNK_SIZE] for i in range(0, len(message), CHUNK_SIZE)]:
                s.write(chunk)
                time.sleep(0.2)
            print(s.readline())
        exitUploader(s)

@click.command()
@click.option("-p", "--port", type=str, default=None,
    help="Specify serial port")
def read(port, dir):
    pass

@click.command()
@acceptsSerialPort
@click.argument("source", type=click.Path(exists=True, file_okay=True, dir_okay=False))
@click.argument("target", type=str)
def push(port, baudrate, source, target):
    with serial.Serial(getPortPath(port), baudrate) as s:
        jumpIntoUploader(s)
        content = base64.b64encode(open(source, "rb").read()).decode("utf-8")
        message = f"PUSH {target} {content}\n".encode("utf-8")
        CHUNK_SIZE = 1024
        for chunk in [message[i:i + CHUNK_SIZE] for i in range(0, len(message), CHUNK_SIZE)]:
            s.write(chunk)
            time.sleep(0.1)
        print(s.readline())
        print(exitUploader(s))

@click.command()
@acceptsSerialPort
@click.argument("source", type=str)
@click.argument("target", type=click.File("wb"))
def pull(port, baudrate, source, target):
    with serial.Serial(getPortPath(port), baudrate) as s:
        jumpIntoUploader(s)
        s.write(f"PULL {source}\n".encode("utf-8"))
        content = s.readline()[:-2]
        target.write(base64.b64decode(content))

@click.command("list")
@acceptsSerialPort
def listContent(port, baudrate):
    with serial.Serial(getPortPath(port), baudrate) as s:
        for l in listTargetEntries(s):
            if l.type == FileType.File:
                print(l.name)

@click.group(())
def cli():
    pass

cli.add_command(sync)
cli.add_command(read)
cli.add_command(push)
cli.add_command(pull)
cli.add_command(listContent)

if __name__ == "__main__":
    cli()