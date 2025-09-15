from setuptools import setup, find_packages

setup(
    name="bloa-lang",
    version="0.1.0",
    description="BLOA scripting language",
    packages=find_packages(),
    include_package_data=True,
    entry_points={
        "console_scripts": [
            "bloa=bloa.cli:main",
        ],
    },
    install_requires=[],
    python_requires=">=3.8",
)