#pragma once
#ifndef _KML_NODE_H_
#define _KML_NODE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Animation.h"
#include "Bound.h"
#include "Compatibility.h"
#include "Material.h"
#include "Mesh.h"
#include "Transform.h"

namespace kml
{
    class Mesh;
    class Bound;
    class Transform;
    class Skin;
    class Node
    {
    public:
        Node();
        void SetName(const std::string& name);
        std::string GetName() const;
        std::string GetPath() const;
        std::string GetOriginalPath() const;
        const std::shared_ptr<Transform>& GetTransform() const;
        std::shared_ptr<Mesh>& GetMesh();
        const std::shared_ptr<Mesh>& GetMesh() const;
        std::shared_ptr<Bound>& GetBound();
        const std::shared_ptr<Bound>& GetBound() const;

        const std::vector<std::shared_ptr<Material> >& GetMaterials() const;
        const std::vector<std::shared_ptr<Animation> >& GetAnimations() const;
        const std::vector<std::shared_ptr<Skin> >& GetSkins() const;
        const std::vector<std::shared_ptr<Node> >& GetChildren() const;

    public:
        void SetTransform(const std::shared_ptr<Transform>& trans);
        void SetPath(const std::string& path);
        void SetOriginalPath(const std::string& path);
        void SetMesh(const std::shared_ptr<Mesh>& mesh);
        void SetBound(const std::shared_ptr<Bound>& bound);
        void AddMaterial(const std::shared_ptr<Material>& material);
        void AddChild(const std::shared_ptr<Node>& child);
        void ClearChildren();
        void AddAnimation(const std::shared_ptr<Animation>& anim);
        void AddSkin(const std::shared_ptr<Skin>& skin);

        void SetVisiblity(bool isVisible);
        bool GetVisibility() const;

    public:
        bool IsLeaf() const;
        bool HasShape() const;

    protected:
        std::string name;
        std::string path;
        std::string original_path;
        std::vector<std::shared_ptr<Node> > children;
        std::shared_ptr<Transform> transform;
        std::shared_ptr<Bound> bound;
        std::shared_ptr<Mesh> mesh;
        std::vector<std::shared_ptr<Material> > materials;
        std::vector<std::shared_ptr<Animation> > animations;
        std::vector<std::shared_ptr<Skin> > skins;
        std::map<std::string, int> iprops;
    };
} // namespace kml

#endif