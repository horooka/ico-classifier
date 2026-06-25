from PIL import Image

# 32x32
base_images = {'user': 'images/user.png',
               'admin': 'images/admin.png',
               'group': 'images/group.png',
               'database': 'images/database.png',
               'server': 'images/server.png'
              }
# 16x16
mod_images = {'add': 'images/add-mod.png',
             'delete': 'images/del-mod.png',
             'gear': 'images/gear-mod.png',
             'contact': 'images/contact-mod.png',
            }

def overlay_modifiers(background_path, modifiers, output_path):
    base_image = Image.open(background_path).convert("RGBA")

    overlay_layer = Image.new('RGBA', base_image.size, (0, 0, 0, 0))

    for mod in modifiers:
        mod_path = mod_images[mod[0]]
        x_pos = mod[1]
        y_pos = mod[2]
        modifier_img = Image.open(mod_path).convert("RGBA")
        overlay_layer.paste(modifier_img, (x_pos, y_pos), modifier_img)

    final_image = Image.alpha_composite(base_image, overlay_layer)
    final_image.convert("RGBA").save(output_path, "PNG")

# base, [[mod, x, y], ...]
combinations = [['user', [['add', 15, 1]]],
                ['user', [['delete', 15, 1]]],
                ['admin', [['add', 15, 1]]],
                ['admin', [['delete', 15, 1]]],
                ['group', [['delete', 15, 1]]],
                ['group', [['add', 15, 1]]],
                ['user', [['contact', 14, 14], ['add', 15, 1]]],
                ['user', [['contact', 14, 14], ['delete', 15, 1]]],
                ['group', [['contact', 14, 14], ['add', 15, 1]]],
                ['group', [['contact', 14, 14], ['delete', 15, 1]]],
                ['database', [['add', 15, 1]]],
                ['database', [['delete', 15, 1]]],
                ['server', [['add', 15, 1]]],
                ['server', [['delete', 15, 1]]],
                ['database', []],
                ['server', []],
                ]

csv = open("synth-data/synth-data.csv", "w")
csv.write("path,name,base,mods,split\n")

for c in combinations:
    output_path = c[0] + "-" + "-".join([x[0] for x in c[1]]) + ".png"
    print(output_path)

    overlay_modifiers(
        background_path=base_images[c[0]], 
        modifiers=c[1],
        output_path="synth-data/" + output_path,
    )

    csv.write(output_path + "," + c[0] + "," + c[0] + "," + ";".join([x[0] for x in c[1]]) + ",train\n")
