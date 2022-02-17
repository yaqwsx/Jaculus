import { parse } from "yaml"
import { readFileSync, existsSync, mkdirSync, openSync } from "fs"
import { resolve } from "path"
import { RethrownError } from "util/index.js"

class Project {
    readonly path: string

    constructor(dir: string) {
        this.path = resolve(process.cwd(), dir)
        if (!existsSync(this.configuration)) {
            throw Error("This folder is not a Jaculus project.")
        }
    }

    get configuration(): string {
        return resolve(this.path, "jac.yml")
    }

    get auxiliaryFolder(): string {
        return resolve(this.path, ".jac")
    }

    get buildFolder(): string {
        return resolve(this.auxiliaryFolder, "build")
    }

    getAuxiliaryFolder(): string {
        try {
            mkdirSync(this.auxiliaryFolder)
        } catch (error) {
            if ((error as any).code !== "EEXIST") // Silent fail for "exists" errors
                throw new RethrownError(error as Error)
        }
        return this.auxiliaryFolder
    }

    getBuildFolder(): string {
        try {
            mkdirSync(this.buildFolder, { recursive: true })
        } catch (error) {
            if ((error as any).code !== "EEXIST") // Silent fail for "exists" errors
                throw new RethrownError(error as Error)
        }
        return this.buildFolder
    }

    getConfiguration() {
        return parse(readFileSync(this.configuration, "utf8"))
    }

    static create(dir: string): Project {
        const path = resolve(process.cwd(), dir)

        try {
            mkdirSync(path, { recursive: true })
        } catch (error) {
            if ((error as any).code !== "EEXIST") // Silent fail for "exists" errors
                throw new RethrownError(error as Error)
        }


        openSync(resolve(path, "jac.yml"), "wx") // fails if jac.yml already exists // FIXME: Do we want this?

        return new Project(path)
    }
}

export { Project }
