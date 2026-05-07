// FontyDE — Decompiler Edition
// web/pre.js
// Injected before the Emscripten module. Exposes fontyde_decompile() to JS.

Module['onRuntimeInitialized'] = function() {
    window.FontyDE = {
        decompile: function(uint8array, filename, opts) {
            opts = opts || {};
            var path = '/input/' + filename;
            try { FS.mkdir('/input'); } catch(e) {}
            try { FS.mkdir('/output'); } catch(e) {}
            FS.writeFile(path, uint8array);

            var args = [path, '-o', '/output'];
            if (opts.noGlyphs)  args.push('--no-glyphs');
            if (opts.noCmap)    args.push('--no-cmap');
            if (opts.noKern)    args.push('--no-kern');
            if (opts.noBitmaps) args.push('--no-bitmaps');

            try {
                Module['callMain'](args);
            } catch(e) {}

            var stem = filename.replace(/\.[^.]+$/, '');
            var outPath = '/output/' + stem + '.fyde';
            try {
                var result = FS.readFile(outPath, { encoding: 'utf8' });
                FS.unlink(outPath);
                return result;
            } catch(e) {
                return null;
            }
        }
    };
    if (typeof window.FontyDEReady === 'function') window.FontyDEReady();
};
