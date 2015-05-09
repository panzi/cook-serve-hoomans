#!/bin/bash

{
printf '.PHONY: all clean\n\nhoomans_opt.png:'

for f in {0..5}; do
	for l in {0..9}; do
		for s in {0..4}; do
			printf ' tmp/hoomans_%s_%s_%s.png' $f $l $s
		done
	done
done

printf '\n\tcp $(shell ls -1 -S tmp/*.png|tail -n 1) $@\n\n'

for f in {0..5}; do
	for l in {0..9}; do
		for s in {0..4}; do
			printf 'tmp/hoomans_%s_%s_%s.png: ../hoomans_post.png Makefile\n' $f $l $s
			printf '\tconvert $< -define png:compression-filter=%s -define png:compression-level=%s -define png:compression-strategy=%s $@\n\n' $f $l $s
		done
	done
done

printf 'clean:\n'
printf '\trm hoomans_opt.png'

for f in {0..5}; do
	for l in {0..9}; do
		for s in {0..4}; do
			printf ' tmp/hoomans_%s_%s_%s.png' $f $l $s
		done
	done
done

printf '\n'

} > Makefile
