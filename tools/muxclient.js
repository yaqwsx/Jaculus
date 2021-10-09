// import FrameParser from './frameparser.mjs'
const SerialPort = require('serialport')
const FrameParser = require('./frameparser')
const port = SerialPort('COM3', {
    baudRate: 921600,
    autoOpen: false
})

const parser = port.pipe(new FrameParser({}))

parser.on('data', console.log)

port.open(function (err) {
    if (err) {
        return console.log('Error opening port: ', err.message)
    }
})
