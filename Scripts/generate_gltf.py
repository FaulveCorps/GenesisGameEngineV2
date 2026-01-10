import json
import struct
import base64

def create_gltf():
    # Geometry: A simple triangle
    # Positions (vec3 float)
    positions = [
        0.0, 1.0, 0.0,  # Top
        -1.0, -1.0, 0.0, # Bottom Left
        1.0, -1.0, 0.0   # Bottom Right
    ]
    
    # Normals (vec3 float) - pointing +Z
    normals = [
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0
    ]
    
    # UVs (vec2 float)
    uvs = [
        0.5, 1.0,
        0.0, 0.0,
        1.0, 0.0
    ]
    
    # Indices (scalar unsigned short)
    indices = [0, 1, 2]

    # Pack data
    # Buffer structure: [Indices] [Positions] [Normals] [UVs]
    # Indices: 3 * 2 bytes = 6 bytes. Padding to 4 bytes alignment -> 8 bytes? 
    # Actually, let's keep them in separate buffer views or just one buffer.
    # Alignment requirements: 
    # accessor.byteOffset must be divisible by componentType size.
    
    buffer_data = bytearray()
    
    # Indices (componentType 5123 = UNSIGNED_SHORT = 2 bytes)
    indices_offset = len(buffer_data)
    for i in indices:
        buffer_data.extend(struct.pack('<H', i))
    
    # Padding for 4-byte alignment of next view (Positions = float = 4 bytes)
    while len(buffer_data) % 4 != 0:
        buffer_data.append(0)
        
    # Positions (componentType 5126 = FLOAT = 4 bytes)
    positions_offset = len(buffer_data)
    for p in positions:
        buffer_data.extend(struct.pack('<f', p))

    # Normals
    normals_offset = len(buffer_data)
    for n in normals:
        buffer_data.extend(struct.pack('<f', n))
        
    # UVs
    uvs_offset = len(buffer_data)
    for u in uvs:
        buffer_data.extend(struct.pack('<f', u))

    buffer_length = len(buffer_data)
    uri = "data:application/octet-stream;base64," + base64.b64encode(buffer_data).decode('utf-8')

    gltf = {
        "asset": { "version": "2.0" },
        "scene": 0,
        "scenes": [ { "nodes": [0] } ],
        "nodes": [ { "mesh": 0 } ],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 1,
                            "NORMAL": 2,
                            "TEXCOORD_0": 3
                        },
                        "indices": 0,
                        "material": 0
                    }
                ]
            }
        ],
        "materials": [
            {
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 0.766, 0.336, 1.0], # Gold-ish
                    "metallicFactor": 1.0,
                    "roughnessFactor": 0.2
                },
                "name": "GoldMaterial"
            }
        ],
        "buffers": [
            {
                "byteLength": buffer_length,
                "uri": uri
            }
        ],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": indices_offset,
                "byteLength": len(indices) * 2,
                "target": 34963 # ELEMENT_ARRAY_BUFFER
            },
            {
                "buffer": 0,
                "byteOffset": positions_offset,
                "byteLength": len(positions) * 4,
                "target": 34962 # ARRAY_BUFFER
            },
            {
                "buffer": 0,
                "byteOffset": normals_offset,
                "byteLength": len(normals) * 4,
                "target": 34962
            },
            {
                "buffer": 0,
                "byteOffset": uvs_offset,
                "byteLength": len(uvs) * 4,
                "target": 34962
            }
        ],
        "accessors": [
            {
                "bufferView": 0,
                "byteOffset": 0,
                "componentType": 5123, # UNSIGNED_SHORT
                "count": len(indices),
                "type": "SCALAR",
                "max": [2],
                "min": [0]
            },
            {
                "bufferView": 1,
                "byteOffset": 0,
                "componentType": 5126, # FLOAT
                "count": int(len(positions) / 3),
                "type": "VEC3",
                "max": [1.0, 1.0, 0.0],
                "min": [-1.0, -1.0, 0.0]
            },
            {
                "bufferView": 2,
                "byteOffset": 0,
                "componentType": 5126, # FLOAT
                "count": int(len(normals) / 3),
                "type": "VEC3"
            },
            {
                "bufferView": 3,
                "byteOffset": 0,
                "componentType": 5126, # FLOAT
                "count": int(len(uvs) / 2),
                "type": "VEC2"
            }
        ]
    }

    with open('Assets/models/pbr_sample.gltf', 'w') as f:
        json.dump(gltf, f, indent=2)
    
    print("Generated Assets/models/pbr_sample.gltf")

if __name__ == "__main__":
    create_gltf()
