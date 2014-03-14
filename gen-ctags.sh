#!/bin/bash
ctags --languages=C++ --exclude=third_party --exclude=.git --exclude=build -R -f .tmp_tags && mv .tmp_tags .tags