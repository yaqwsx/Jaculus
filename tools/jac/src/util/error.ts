import { ExtendableError } from "ts-error"

class JacError extends ExtendableError { }

class SimpleError extends JacError { }

class RethrownError extends JacError {
    original_error: ExtendableError
    stack_before_rethrow?: string

    constructor(error: ExtendableError, ...params: any[]) {
        super(...params)
        this.original_error = error
        this.name = this.constructor.name
        this.stack_before_rethrow = this.stack
        const message_lines = (this.message.match(/\n/g) || []).length + 1
        if (this.stack)
            this.stack = this.stack.split('\n').slice(0, message_lines + 1).join('\n') + '\n' + error.stack
    }
}

export { JacError, RethrownError, SimpleError }
