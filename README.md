# Ico classifier

## Train

Generation of synthetic data (base image, modifiers and their positions)

- Ico name format - `$BaseName-$ModName1-...-$ModNameX`
- Base image name format - `$BaseName`
- Modifier image name format - `$ModName-mod`

## Server

Fastapi server with pytorch inference

## Client

Gtkmm3 client app

- Server domain - asked on first launch and cached in ~/.config/ico-classifier.ini as "Domain"
- Ico dir - asked on first launch and cached in ~/.config/ico-classifier.ini as "IcoDir"
