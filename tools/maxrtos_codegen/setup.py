from setuptools import setup, find_packages

setup(
    name="maxrtos_codegen",
    version="0.1.0",
    packages=find_packages(),
    package_data={"maxrtos_codegen": ["templates/*.j2"]},
    install_requires=["jinja2>=3.0"],
    entry_points={
        "console_scripts": ["maxrtos-codegen=maxrtos_codegen.cli:main"],
    },
    python_requires=">=3.9",
)
