import {pathToFileURL} from 'node:url';

if (pathToFileURL(import.meta.filename).href !== import.meta.url)
    throw Error('Main-process canonical file URL');
export default {};
