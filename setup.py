from setuptools import Extension, setup

stubs = {
    'packages': ['texatlas-stubs'],
    'package_data': {'texatlas-stubs': ['__init__.pyi']},
    'include_package_data': True,
}

ext = Extension(
    name='texatlas',
    sources=['texatlas.cpp'],
    py_limited_api=True,
    define_macros=[
        ('Py_LIMITED_API', 0x03070000),
    ],
)

with open('README.md') as readme:
    long_description = readme.read()

setup(
    name='texatlas',
    version='0.2.0',
    ext_modules=[ext],
    license='MIT',
    python_requires='>=3.7',
    platforms=['any'],
    description='Atlas Texture generator for Images and Fonts',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='Szabolcs Dombi',
    author_email='szabolcs@szabolcsdombi.com',
    url='https://github.com/szabolcsdombi/texatlas/',
    project_urls={
        # 'Documentation': 'https://texatlas.readthedocs.io/',
        'Source': 'https://github.com/szabolcsdombi/texatlas/',
        'Bug Tracker': 'https://github.com/szabolcsdombi/texatlas/issues/',
    },
    classifiers=[
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: Implementation :: CPython',
        'License :: OSI Approved',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Topic :: Multimedia',
        'Topic :: Multimedia :: Graphics',
        'Environment :: Win32 (MS Windows)',
        'Environment :: X11 Applications',
        'Environment :: MacOS X',
        'Typing :: Typed',
    ],
    keywords=[
        'font',
        'atlas',
        'truetype',
        'rectpack',
    ],
    **stubs,
)
