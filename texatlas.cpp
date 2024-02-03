#include <Python.h>

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

struct ModuleState {
    PyTypeObject * Packer_type;
    PyObject * printable;
    PyObject * io;
};

struct Packer {
    PyObject_HEAD
    ModuleState * state;
    PyObject * fonts;
    PyObject * images;
};

static int * int_list(PyObject * lst) {
    int size = (int)PyList_Size(lst);
    int * ints = (int *)malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        ints[i] = PyLong_AsLong(PyList_GetItem(lst, i));
    }
    return ints;
}

static Packer * meth_packer(PyObject * self, PyObject * args) {
    ModuleState * state = (ModuleState *)PyModule_GetState(self);
    Packer * packer = PyObject_New(Packer, state->Packer_type);
    packer->state = state;
    packer->fonts = PyList_New(0);
    packer->images = PyList_New(0);
    return packer;
}

static PyObject * Packer_meth_font(Packer * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"name", "font", "size", "glyphs", NULL};

    PyObject * name;
    PyObject * font;
    int size;
    PyObject * glyphs = self->state->printable;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOi|O", (char **)keywords, &name, &font, &size, &glyphs)) {
        return NULL;
    }

    if (PyUnicode_Check(font)) {
        PyObject * fd = PyObject_CallMethod(self->state->io, "open", "Os", font, "rb");
        if (!fd) {
            return NULL;
        }
        font = PyObject_CallMethod(fd, "read", NULL);
        PyObject_CallMethod(fd, "close", NULL);
    } else {
        font = PyBytes_FromObject(font);
        if (!font) {
            return NULL;
        }
    }

    glyphs = PySequence_List(glyphs);
    if (!glyphs) {
        return NULL;
    }

    PyList_Append(self->fonts, Py_BuildValue("(ONiN)", name, font, size, glyphs));
    Py_RETURN_NONE;
}

static PyObject * Packer_meth_image(Packer * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"name", "image", "size", NULL};

    PyObject * name;
    PyObject * image;
    int width = 0;
    int height = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|(ii)", (char **)keywords, &name, &image, &width, &height)) {
        return NULL;
    }

    if (PyUnicode_Check(image)) {
        PyObject * fd = PyObject_CallMethod(self->state->io, "open", "Os", image, "rb");
        if (!fd) {
            return NULL;
        }
        PyObject * raw = PyObject_CallMethod(fd, "read", NULL);
        PyObject_CallMethod(fd, "close", NULL);
        unsigned char * ptr = (unsigned char *)PyBytes_AsString(raw);
        int size = (int)PyBytes_Size(raw);
        unsigned char * data = stbi_load_from_memory(ptr, size, &width, &height, NULL, STBI_rgb_alpha);
        if (!data) {
            return NULL;
        }
        Py_DECREF(raw);
        image = PyBytes_FromStringAndSize((char *)data, width * height * 4);
        stbi_image_free(data);
    } else {
        if (!width || !height) {
            return NULL;
        }
        image = PyBytes_FromObject(image);
        if (!image) {
            return NULL;
        }
    }

    PyList_Append(self->images, Py_BuildValue("(ONii)", name, image, width, height));
    Py_RETURN_NONE;
}

static PyObject * Packer_meth_build(Packer * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"size", "oversampling", "padding", NULL};

    int width, height;
    int oversampling = 1;
    int padding = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "(ii)|ii", (char **)keywords, &width, &height, &oversampling, &padding)) {
        return NULL;
    }

    int num_fonts = (int)PyList_Size(self->fonts);
    int num_images = (int)PyList_Size(self->images);

    int num_rects = num_images;
    for (int i = 0; i < num_fonts; ++i) {
        PyObject * font = PyList_GetItem(self->fonts, i);
        num_rects += (int)PyList_Size(PyTuple_GetItem(font, 3));
    }

    PyObject * pixels = PyBytes_FromStringAndSize(NULL, width * height * 4);
    unsigned * write = (unsigned *)PyBytes_AsString(pixels);

    unsigned char * atlas = (unsigned char *)malloc(width * height * 4);
    memset(atlas, 0, width * height * 4);

    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, atlas, width, height, 0, padding, NULL);
    stbtt_PackSetOversampling(&pc, oversampling, oversampling);

    stbtt_packedchar * chars = (stbtt_packedchar *)malloc(num_rects * sizeof(stbtt_packedchar));
    stbrp_rect * rects = (stbrp_rect *)malloc(num_rects * sizeof(stbrp_rect));

    stbtt_fontinfo * infos = (stbtt_fontinfo *)malloc(num_fonts * sizeof(stbtt_fontinfo));
    stbtt_pack_range * ranges = (stbtt_pack_range *)malloc(num_fonts * sizeof(stbtt_pack_range));

    int last_glyph_index = 0;
    for (int i = 0; i < num_fonts; ++i) {
        PyObject * font = PyList_GetItem(self->fonts, i);
        float size = (float)PyLong_AsLong(PyTuple_GetItem(font, 2));
        int * glyphs = int_list(PyTuple_GetItem(font, 3));
        int num_glyphs = (int)PyList_Size(PyTuple_GetItem(font, 3));
        ranges[i] = {size, 0, glyphs, num_glyphs, chars + last_glyph_index};

        unsigned char * font_data = (unsigned char *)PyBytes_AsString(PyTuple_GetItem(font, 1));
        stbtt_InitFont(&infos[i], font_data, stbtt_GetFontOffsetForIndex(font_data, 0));
        stbtt_PackFontRangesGatherRects(&pc, &infos[i], &ranges[i], 1, rects + last_glyph_index);
        last_glyph_index += num_glyphs;
    }

    for (int i = 0; i < num_images; ++i) {
        PyObject * image = PyList_GetItem(self->images, i);
        int image_width = (int)PyLong_AsLong(PyTuple_GetItem(image, 2));
        int image_height = (int)PyLong_AsLong(PyTuple_GetItem(image, 3));
        rects[last_glyph_index + i] = {0, image_width + padding, image_height + padding, 0, 0, 0};
    }

    stbrp_pack_rects((stbrp_context *)pc.pack_info, rects, num_rects);

    last_glyph_index = 0;
    for (int i = 0; i < num_fonts; ++i) {
        PyObject * font = PyList_GetItem(self->fonts, i);
        int num_glyphs = (int)PyList_Size(PyTuple_GetItem(font, 3));
        stbtt_PackFontRangesRenderIntoRects(&pc, &infos[i], &ranges[i], 1, rects + last_glyph_index);
        last_glyph_index += num_glyphs;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            write[y * width + x] = atlas[y * width + x] << 24 | 0xffffff;
        }
    }

    for (int i = 0; i < num_images; ++i) {
        PyObject * image = PyList_GetItem(self->images, i);
        unsigned * read = (unsigned *)PyBytes_AsString(PyTuple_GetItem(image, 1));
        stbrp_rect r = rects[last_glyph_index + i];
        int dx = r.x + padding;
        int dy = r.y + padding;
        int w = r.w - padding;
        int h = r.h - padding;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int src = (h - y - 1) * w + x;
                int dst = (dy + y) * width + (dx + x);
                write[dst] = read[src];
            }
        }
    }

    PyObject * bbox = PyUnicode_FromString("bbox");
    PyObject * uvbox = PyUnicode_FromString("uvbox");
    PyObject * offset = PyUnicode_FromString("offset");
    PyObject * size = PyUnicode_FromString("size");
    PyObject * advance = PyUnicode_FromString("advance");

    PyObject * lookup = PyDict_New();

    last_glyph_index = 0;
    for (int i = 0; i < num_fonts; ++i) {
        PyObject * font = PyList_GetItem(self->fonts, i);
        int num_glyphs = (int)PyList_Size(PyTuple_GetItem(font, 3));
        PyObject * glyphs_table = PyDict_New();
        for (int j = 0; j < num_glyphs; ++j) {
            stbtt_packedchar c = chars[last_glyph_index + j];
            int box[4] = {c.x0, c.y0, c.x1, c.y1};
            for (int a = c.y0, b = c.y1 - 1; a < b; ++a, --b) {
                for (int x = c.x0; x < c.x1; ++x) {
                    unsigned temp = write[a * width + x];
                    write[a * width + x] = write[b * width + x];
                    write[b * width + x] = temp;
                }
            }
            float uv[4] = {
                (float)box[0] / (float)width,
                (float)box[1] / (float)height,
                (float)box[2] / (float)width,
                (float)box[3] / (float)height,
            };
            float sx = (float)(c.x1 - c.x0) / (float)oversampling;
            float sy = (float)(c.y1 - c.y0) / (float)oversampling;
            PyObject * glyph_info = Py_BuildValue(
                "{O(iiii)O(ffff)O(ff)O(ff)Of}",
                bbox, box[0], box[1], box[2], box[3],
                uvbox, uv[0], uv[1], uv[2], uv[3],
                offset, c.xoff, -c.yoff - sy,
                size, sx, sy,
                advance, c.xadvance
            );
            PyObject * key = PyList_GetItem(PyTuple_GetItem(font, 3), j);
            PyDict_SetItem(glyphs_table, key, glyph_info);
            Py_DECREF(glyph_info);
        }
        PyObject * name = PyTuple_GetItem(font, 0);
        PyDict_SetItem(lookup, name, glyphs_table);
        last_glyph_index += num_glyphs;
    }

    for (int i = 0; i < num_images; ++i) {
        PyObject * image = PyList_GetItem(self->images, i);
        stbrp_rect r = rects[last_glyph_index + i];
        int box[4] = {r.x + padding, r.y + padding, r.x + r.w, r.y + r.h};
        float uv[4] = {
            (float)box[0] / (float)width,
            (float)box[1] / (float)height,
            (float)box[2] / (float)width,
            (float)box[3] / (float)height,
        };
        float sx = (float)(r.w - padding);
        float sy = (float)(r.h - padding);
        PyObject * image_info = Py_BuildValue(
            "{O(iiii)O(ffff)O(ff)}",
            bbox, box[0], box[1], box[2], box[3],
            uvbox, uv[0], uv[1], uv[2], uv[3],
            size, sx, sy
        );
        PyObject * name = PyTuple_GetItem(image, 0);
        PyDict_SetItem(lookup, name, image_info);
        Py_DECREF(image_info);
    }

    Py_DECREF(bbox);
    Py_DECREF(uvbox);
    Py_DECREF(offset);
    Py_DECREF(size);
    Py_DECREF(advance);

    for (int i = 0; i < num_fonts; ++i) {
        free(ranges[i].array_of_unicode_codepoints);
    }

    free(atlas);
    free(chars);
    free(rects);
    free(infos);
    free(ranges);

    return Py_BuildValue("(NN)", pixels, lookup);
}

static void Packer_dealloc(Packer * self) {
    Py_DECREF(self->images);
    Py_DECREF(self->fonts);
    PyObject_Del(self);
}

static PyMethodDef Packer_methods[] = {
    {"font", (PyCFunction)Packer_meth_font, METH_VARARGS | METH_KEYWORDS},
    {"image", (PyCFunction)Packer_meth_image, METH_VARARGS | METH_KEYWORDS},
    {"build", (PyCFunction)Packer_meth_build, METH_VARARGS | METH_KEYWORDS},
    {0},
};

static PyType_Slot Packer_slots[] = {
    {Py_tp_methods, Packer_methods},
    {Py_tp_dealloc, (void *)Packer_dealloc},
    {0},
};

static PyType_Spec Packer_spec = {"Packer", sizeof(Packer), 0, Py_TPFLAGS_DEFAULT, Packer_slots};

static int module_exec(PyObject * self) {
    ModuleState * state = (ModuleState *)PyModule_GetState(self);
    state->Packer_type = (PyTypeObject *)PyType_FromSpec(&Packer_spec);

    state->io = PyImport_ImportModule("io");
    if (!state->io) {
        return NULL;
    }

    state->printable = PyList_New(95);
    for (int i = 0; i < 95; ++i) {
        PyList_SetItem(state->printable, i, PyLong_FromLong(i + 32));
    }

    PyModule_AddObject(self, "Packer", (PyObject *)state->Packer_type);
    PyModule_AddObject(self, "__version__", PyUnicode_FromString("0.2.0"));
    return 0;
}

static PyMethodDef module_methods[] = {
    {"packer", (PyCFunction)meth_packer, METH_NOARGS},
    {0},
};

static PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, (void *)module_exec},
    {0},
};

static PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT, "texatlas", NULL, sizeof(ModuleState), module_methods, module_slots, NULL, NULL, NULL,
};

extern PyObject * PyInit_texatlas() {
    return PyModuleDef_Init(&module_def);
}
