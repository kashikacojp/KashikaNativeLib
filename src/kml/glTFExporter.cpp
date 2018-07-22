#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#define NOMINMAX
#include <windows.h>
#endif

#include "glTFExporter.h"

#include "TriangulateMesh.h"
#include "Options.h"
#include "SaveToDraco.h"

#include <climits>
#include <vector>
#include <set>
#include <fstream>

#include "gltfConstants.h"

#include <picojson/picojson.h>


namespace {
	enum ImageFormat {
		FORMAT_JPEG = 0,
		FORMAT_PNG,
		FORMAT_BMP,
		FORMAT_GIF
	};
}

namespace kml
{
	static
	std::string IToS(int n)
	{
		char buffer[16] = {};
#ifdef _WIN32
		_snprintf(buffer, 16, "%03d", n);
#else
		snprintf(buffer, 16, "%03d", n);
#endif
		return buffer;
	}

	static
	std::string GetBaseDir(const std::string& filepath)
	{
#ifdef _WIN32
		char dir[MAX_PATH + 1] = {};
		char drv[MAX_PATH + 1] = {};
		_splitpath(filepath.c_str(), drv, dir, NULL, NULL);
		return std::string(drv) + std::string(dir);
#else
		if (filepath.find_last_of("/") != std::string::npos)
			return filepath.substr(0, filepath.find_last_of("/")) + "/";
#endif
		return "";
	}

	static
	std::string RemoveExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(0, filepath.find_last_of("."));
		return filepath;
	}

	static
	std::string GetBaseName(const std::string& filepath)
	{
#ifdef _WIN32
		char fname[MAX_PATH + 1] = {};
		_splitpath(filepath.c_str(), NULL, NULL, fname, NULL);
		return fname;
#else
		if (filepath.find_last_of("/") != std::string::npos)
			return RemoveExt(filepath.substr(filepath.find_last_of("/") + 1));
#endif
		return filepath;
	}

	static
	std::string GetImageID(const std::string& imagePath)
	{
		return GetBaseName(imagePath);
	}

	static
	std::string GetTextureID(const std::string& imagePath)
	{
		return "texture_" + GetImageID(imagePath);
	}

	static
	std::string GetFileExtName(const std::string& path)
	{
#ifdef _WIN32
		char szFname[_MAX_FNAME];
		char szExt[_MAX_EXT];
		_splitpath(path.c_str(), NULL, NULL, szFname, szExt);
		std::string strRet1;
		strRet1 += szFname;
		strRet1 += szExt;
		return strRet1;
#else
		int i = path.find_last_of('/');
		if (i != std::string::npos)
		{
			return path.substr(i + 1);
		}
		else
		{
			return path;
		}
#endif
	}

	static
	void GetTextures(std::set<std::string>& texture_set, const std::vector< std::shared_ptr<::kml::Material> >& materials)
	{
		for (size_t j = 0; j < materials.size(); j++)
		{
			const auto& mat = materials[j];
			auto keys = mat->GetStringKeys();
			for (int i = 0; i < keys.size(); i++)
			{
				std::string texpath = mat->GetTextureName(keys[i]);
				texture_set.insert(texpath);
			}
		}
	}

	static
	std::string GetExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(filepath.find_last_of("."));
		return "";
	}

	static
	unsigned int GetImageFormat(const std::string& path)
	{
		std::string ext = GetExt(path);
		if (ext == ".jpg" || ext == ".jpeg")
		{
			return FORMAT_JPEG;
		}
		else if (ext == ".png")
		{
			return FORMAT_PNG;
		}
		else if (ext == ".bmp")
		{
			return FORMAT_BMP;
		}
		else if (ext == ".gif")
		{
			return FORMAT_GIF;
		}
		return FORMAT_JPEG;
	}

	namespace gltf
	{
		class Buffer
		{
		public:
			Buffer(const std::string& name, int index, bool is_draco = false, bool is_union_buffer_draco = false)
				:name_(name), index_(index), is_draco_(is_draco), is_union_buffer_draco_(is_union_buffer_draco)
			{}
			const std::string& GetName()const
			{
				return name_;
			}
			int GetIndex()const
			{
				return index_;
			}
			std::string GetURI()const
			{
				if (!is_draco_)
				{
					return name_ + ".bin";
				}
				else
				{
					return this->GetCompressDracoURI();
				}
			}
			std::string GetCompressDracoURI()const
			{
				if (is_union_buffer_draco_)
				{
					return name_ + ".bin";
				}
				else
				{
					return name_ + std::string("_") + IToS(index_) + ".bin";
				}
			}
			void AddBytes(const unsigned char bytes[], size_t sz)
			{
				size_t offset = bytes_.size();
				bytes_.resize(offset + sz);
				memcpy(&bytes_[offset], bytes, sz);
			}
			size_t GetSize()const
			{
				return bytes_.size();
			}
			size_t GetByteLength()const
			{
				return GetSize();
			}
			const unsigned char* GetBytesPtr()const
			{
				return &bytes_[0];
			}
			bool IsDraco()const
			{
				return is_draco_;
			}
		protected:
			std::string name_;
			int index_;
			bool is_draco_;
			bool is_union_buffer_draco_;
			std::vector<unsigned char> bytes_;
		};

		class BufferView
		{
		public:
			BufferView(const std::string& name, int index)
				:name_(name), index_(index)
			{}
			const std::string& GetName()const
			{
				return name_;
			}
			int GetIndex()const
			{
				return index_;
			}
			void SetBuffer(const std::shared_ptr<Buffer>& bv)
			{
				buffer_ = bv;
			}
			const std::shared_ptr<Buffer>& GetBuffer()const
			{
				return buffer_;
			}
			void SetByteOffset(size_t sz)
			{
				byteOffset_ = sz;
			}
			void SetByteLength(size_t sz)
			{
				byteLength_ = sz;
			}
			size_t GetByteOffset()const
			{
				return byteOffset_;
			}
			size_t GetByteLength()const
			{
				return byteLength_;
			}
			void SetTarget(int t)
			{
				target_ = t;
			}
			int GetTarget()const
			{
				return target_;
			}
		protected:
			std::string name_;
			int index_;
			std::shared_ptr<Buffer> buffer_;
			size_t byteOffset_;
			size_t byteLength_;
			int target_;
		};


		class Accessor
		{
		public:
			Accessor(const std::string& name, int index)
				:name_(name), index_(index)
			{}
			const std::string& GetName()const
			{
				return name_;
			}
			int GetIndex()const
			{
				return index_;
			}
			void SetBufferView(const std::shared_ptr<BufferView>& bv)
			{
				bufferView_ = bv;
			}
			const std::shared_ptr<BufferView>& GetBufferView()const
			{
				return bufferView_;
			}
			void Set(const std::string& key, const picojson::value& v)
			{
				obj_[key] = v;
			}
			picojson::value Get(const std::string& key)const
			{
				picojson::object::const_iterator it = obj_.find(key);
				if (it != obj_.end())
				{
					return it->second;
				}
				else
				{
					return picojson::value();
				}
			}
		protected:
			std::string name_;
			int index_;
			std::shared_ptr<BufferView> bufferView_;
			picojson::object obj_;
		};


		class Mesh
		{
		public:
			Mesh(const std::string& name, int index)
				:name_(name), index_(index)
			{
				mode_ = GLTF_MODE_TRIANGLES;
			}
			const std::string& GetName()const
			{
				return name_;
			}
			int GetIndex()const
			{
				return index_;
			}
			int GetMode()const
			{
				return mode_;
			}
			void SetMaterialID(int id)
			{
				materialID_ = id;
			}
			int GetMaterialID()
			{
				return materialID_;
			}
			std::shared_ptr<Accessor> GetIndices()
			{
				return accessors_["indices"];
			}
			void SetAccessor(const std::string& name, const std::shared_ptr<Accessor>& acc)
			{
				accessors_[name] = acc;
			}
			std::shared_ptr<Accessor> GetAccessor(const std::string& name)
			{
				return accessors_[name];
			}
		protected:
			std::string name_;
			int index_;
			int mode_;
			int materialID_;
			std::map<std::string, std::shared_ptr<Accessor> > accessors_;
		};

		class Node
		{
		public:
			Node(const std::string& name, int index)
				:name_(name), index_(index)
			{}
			const std::string& GetName()const
			{
				return name_;
			}
			int GetIndex()const
			{
				return index_;
			}
			void SetMesh(const std::shared_ptr<Mesh>& mesh)
			{
				mesh_ = mesh;
			}
			const std::shared_ptr<Mesh>& GetMesh()const
			{
				return mesh_;
			}
		protected:
			std::string name_;
			int index_;
			std::shared_ptr<Mesh> mesh_;
		};

		static
		void GetMinMax(float min[], float max[], const std::vector<float>& verts, int n)
		{
			for (int i = 0; i<n; i++)
			{
				min[i] = std::numeric_limits<float>::max();
				max[i] = std::numeric_limits<float>::min();
			}
			size_t sz = verts.size() / n;
			for (size_t i = 0; i < sz; i++)
			{
				for (int j = 0; j < n; j++)
				{
					min[j] = std::min<float>(min[j], verts[n * i + j]);
					max[j] = std::max<float>(max[j], verts[n * i + j]);
				}
			}
		}

		static
		void GetMinMax(unsigned int& min, unsigned int& max, const std::vector<unsigned int>& verts)
		{
			{
				min = std::numeric_limits<unsigned int>::max();
				max = 0;
			}
			size_t sz = verts.size();
			for (size_t i = 0; i < sz; i++)
			{
				min = std::min<unsigned int>(min, verts[i]);
				max = std::max<unsigned int>(max, verts[i]);
			}
		}

		static
		picojson::array ConvertToArray(float v[], int n)
		{
			picojson::array a;
			for (int j = 0; j<n; j++)
			{
				a.push_back(picojson::value(v[j]));
			}
			return a;
		}

		class ObjectRegister
		{
		public:
			ObjectRegister(const std::string& basename)
			{
				//int nBuffer = nodes_.size();
				//std::string name = "buffer_" + IToS(nBuffer);
				basename_ = basename;
				//buffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename, 0)));
			}
			void RegisterObject(const std::shared_ptr<::kml::Node>& in_node)
			{
				int nNode = nodes_.size();
				std::string nodeName = in_node->GetName();
				std::shared_ptr<Node> node(new Node(nodeName, nNode));

				int nMesh = meshes_.size();
				std::string meshName = "mesh_" + IToS(nMesh);
				std::shared_ptr<Mesh> mesh(new Mesh(meshName, nMesh));


				const std::shared_ptr<::kml::Mesh>& in_mesh = in_node->GetMesh();

				if (in_mesh->materials.size())
				{
					int material_id = in_mesh->materials[0];
					mesh->SetMaterialID(material_id);
				}
				else
				{
					int material_id = 0;
					mesh->SetMaterialID(material_id);
				}

				std::vector<unsigned int> indices(in_mesh->pos_indices.size());
				for (size_t i = 0; i < indices.size(); i++)
				{
					indices[i] = (unsigned int)in_mesh->pos_indices[i];
				}

				std::vector<float> positions(in_mesh->positions.size() * 3);
				for (size_t i = 0; i < in_mesh->positions.size(); i++)
				{
					positions[3 * i + 0] = (float)in_mesh->positions[i][0];
					positions[3 * i + 1] = (float)in_mesh->positions[i][1];
					positions[3 * i + 2] = (float)in_mesh->positions[i][2];
				}

				std::vector<float> normals(in_mesh->normals.size() * 3);
				for (size_t i = 0; i < in_mesh->normals.size(); i++)
				{
					normals[3 * i + 0] = (float)in_mesh->normals[i][0];
					normals[3 * i + 1] = (float)in_mesh->normals[i][1];
					normals[3 * i + 2] = (float)in_mesh->normals[i][2];
				}
				for (size_t i = 0; i < in_mesh->normals.size(); i++)
				{
					float x = normals[3 * i + 0];
					float y = normals[3 * i + 1];
					float z = normals[3 * i + 2];
					float l = (x*x + y * y + z * z);
					if (fabs(l) > 1e-6f)
					{
						l = 1.0f / std::sqrt(l);
						normals[3 * i + 0] = x * l;
						normals[3 * i + 1] = y * l;
						normals[3 * i + 2] = z * l;
					}
				}

				std::vector<float> texcoords;
				if (in_mesh->texcoords.size() > 0)
				{
					texcoords.resize(in_mesh->texcoords.size() * 2);
					for (size_t i = 0; i < in_mesh->texcoords.size(); i++)
					{
						texcoords[2 * i + 0] = (float)in_mesh->texcoords[i][0];
						texcoords[2 * i + 1] = (float)in_mesh->texcoords[i][1];
					}
				}

				int nAcc = accessors_.size();
				{
					//indices
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(indices);
					acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(indices.size())));
					acc->Set("type", picojson::value("SCALAR"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_UNSIGNED_INT));//5125
					acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));

					unsigned int min, max;
					GetMinMax(min, max, indices);
					picojson::array amin, amax;
					amin.push_back(picojson::value((double)min));
					amax.push_back(picojson::value((double)max));
					acc->Set("min", picojson::value(amin));
					acc->Set("max", picojson::value(amax));

					accessors_.push_back(acc);
					mesh->SetAccessor("indices", acc);
					nAcc++;
				}
				{
					//normal
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(normals);
					acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(normals.size() / 3)));
					acc->Set("type", picojson::value("VEC3"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					if (normals.size())
					{
						GetMinMax(min, max, normals, 3);
					}
					acc->Set("min", picojson::value(ConvertToArray(min, 3)));
					acc->Set("max", picojson::value(ConvertToArray(max, 3)));

					accessors_.push_back(acc);
					mesh->SetAccessor("NORMAL", acc);
					nAcc++;
				}
				{
					//position
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(positions);
					acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(positions.size() / 3)));
					acc->Set("type", picojson::value("VEC3"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					GetMinMax(min, max, positions, 3);
					acc->Set("min", picojson::value(ConvertToArray(min, 3)));
					acc->Set("max", picojson::value(ConvertToArray(max, 3)));

					accessors_.push_back(acc);
					mesh->SetAccessor("POSITION", acc);
					nAcc++;
				}
				if (texcoords.size() > 0)
				{
					//texcoord
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(texcoords);
					acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(texcoords.size() / 2)));
					acc->Set("type", picojson::value("VEC2"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)2 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					GetMinMax(min, max, texcoords, 2);
					acc->Set("min", picojson::value(ConvertToArray(min, 2)));
					acc->Set("max", picojson::value(ConvertToArray(max, 2)));

					accessors_.push_back(acc);
					mesh->SetAccessor("TEXCOORD_0", acc);
					nAcc++;
				}
				node->SetMesh(mesh);
				this->meshes_.push_back(mesh);

				this->AddNode(node);
			}

			void RegisterObjectDraco(const std::shared_ptr<::kml::Node>& in_node, bool is_union_buffer = false)
			{
				int nNode = nodes_.size();
				std::string nodeName = in_node->GetName();
				std::shared_ptr<Node> node(new Node(nodeName, nNode));

				int nMesh = meshes_.size();
				std::string meshName = "mesh_" + IToS(nMesh);
				std::shared_ptr<Mesh> mesh(new Mesh(meshName, nMesh));

				const std::shared_ptr<::kml::Mesh>& in_mesh = in_node->GetMesh();

				if (in_mesh->materials.size())
				{
					int material_id = in_mesh->materials[0];
					mesh->SetMaterialID(material_id);
				}
				else
				{
					int material_id = 0;
					mesh->SetMaterialID(material_id);
				}

				std::shared_ptr<BufferView> bufferView;
				{
					size_t offset = 0;
					if (is_union_buffer)
					{
						if (!dracoBuffers_.empty())
						{
							offset = dracoBuffers_[0]->GetSize();
						}
					}
					std::shared_ptr<Buffer> buffer = this->AddBufferDraco(in_node->GetMesh(), is_union_buffer);
					if (buffer.get())
					{
						bufferView = this->AddBufferViewDraco(buffer, offset);
					}
				}

				std::vector<unsigned int> indices(in_mesh->pos_indices.size());
				for (size_t i = 0; i < indices.size(); i++)
				{
					indices[i] = (unsigned int)in_mesh->pos_indices[i];
				}

				std::vector<float> positions(in_mesh->positions.size() * 3);
				for (size_t i = 0; i < in_mesh->positions.size(); i++)
				{
					positions[3 * i + 0] = (float)in_mesh->positions[i][0];
					positions[3 * i + 1] = (float)in_mesh->positions[i][1];
					positions[3 * i + 2] = (float)in_mesh->positions[i][2];
				}

				std::vector<float> normals(in_mesh->normals.size() * 3);
				for (size_t i = 0; i < in_mesh->normals.size(); i++)
				{
					normals[3 * i + 0] = (float)in_mesh->normals[i][0];
					normals[3 * i + 1] = (float)in_mesh->normals[i][1];
					normals[3 * i + 2] = (float)in_mesh->normals[i][2];
				}
				for (size_t i = 0; i < in_mesh->normals.size(); i++)
				{
					float x = normals[3 * i + 0];
					float y = normals[3 * i + 1];
					float z = normals[3 * i + 2];
					float l = (x*x + y * y + z * z);
					if (fabs(l) > 1e-6f)
					{
						l = 1.0f / std::sqrt(l);
						normals[3 * i + 0] = x * l;
						normals[3 * i + 1] = y * l;
						normals[3 * i + 2] = z * l;
					}
				}

				std::vector<float> texcoords;
				if (in_mesh->texcoords.size() > 0)
				{
					texcoords.resize(in_mesh->texcoords.size() * 2);
					for (size_t i = 0; i < in_mesh->texcoords.size(); i++)
					{
						texcoords[2 * i + 0] = (float)in_mesh->texcoords[i][0];
						texcoords[2 * i + 1] = (float)in_mesh->texcoords[i][1];
					}
				}

				int nAcc = accessors_.size();
				{
					//indices
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					//acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(indices.size())));
					acc->Set("type", picojson::value("SCALAR"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_UNSIGNED_INT));//5125
					//acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));

					unsigned int min, max;
					GetMinMax(min, max, indices);
					picojson::array amin, amax;
					amin.push_back(picojson::value((double)min));
					amax.push_back(picojson::value((double)max));
					acc->Set("min", picojson::value(amin));
					acc->Set("max", picojson::value(amax));

					accessors_.push_back(acc);
					mesh->SetAccessor("indices", acc);
					nAcc++;
				}
				{
					//normal
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					//acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(normals.size() / 3)));
					acc->Set("type", picojson::value("VEC3"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					//acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					if (normals.size())
					{
						GetMinMax(min, max, normals, 3);
					}
					acc->Set("min", picojson::value(ConvertToArray(min, 3)));
					acc->Set("max", picojson::value(ConvertToArray(max, 3)));

					accessors_.push_back(acc);
					mesh->SetAccessor("NORMAL", acc);
					nAcc++;
				}
				{
					//position
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					//acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(positions.size() / 3)));
					acc->Set("type", picojson::value("VEC3"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					//acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					GetMinMax(min, max, positions, 3);
					acc->Set("min", picojson::value(ConvertToArray(min, 3)));
					acc->Set("max", picojson::value(ConvertToArray(max, 3)));

					accessors_.push_back(acc);
					mesh->SetAccessor("POSITION", acc);
					nAcc++;
				}
				if (texcoords.size() > 0)
				{
					//texcoord
					std::string accName = "accessor_" + IToS(nAcc);//
					std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
					//acc->SetBufferView(bufferView);
					acc->Set("count", picojson::value((double)(texcoords.size() / 2)));
					acc->Set("type", picojson::value("VEC2"));
					acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
					//acc->Set("byteOffset", picojson::value((double)0));
					//acc->Set("byteStride", picojson::value((double)2 * sizeof(float)));

					float min[3] = {}, max[3] = {};
					GetMinMax(min, max, texcoords, 2);
					acc->Set("min", picojson::value(ConvertToArray(min, 2)));
					acc->Set("max", picojson::value(ConvertToArray(max, 2)));

					accessors_.push_back(acc);
					mesh->SetAccessor("TEXCOORD_0", acc);
					nAcc++;
				}

				node->SetMesh(mesh);
				this->meshes_.push_back(mesh);

				this->AddNode(node);
			}
		public:
			const std::vector<std::shared_ptr<Node> >& GetNodes()const
			{
				return nodes_;
			}
			const std::vector<std::shared_ptr<Mesh> >& GetMeshes()const
			{
				return meshes_;
			}
			const std::vector<std::shared_ptr<Accessor> >& GetAccessors()const
			{
				return accessors_;
			}
			const std::vector<std::shared_ptr<BufferView> >& GetBufferViews()const
			{
				return bufferViews_;
			}
			const std::vector<std::shared_ptr<Buffer> >& GetBuffers()const
			{
				return buffers_;
			}
		public:
			const std::vector<std::shared_ptr<Buffer> >& GetBuffersDraco()const
			{
				return dracoBuffers_;
			}
		protected:
			std::shared_ptr<Buffer> GetLastBinBuffer()
			{
				if (buffers_.empty())
				{
					buffers_.push_back( std::shared_ptr<Buffer>( new Buffer(basename_, 0)) );
				}
				return buffers_.back();
			}
			void AddNode(const std::shared_ptr<Node>& node)
			{
				nodes_.push_back(node);
			}
			const std::shared_ptr<BufferView>& AddBufferView(const std::vector<float>& vec)
			{
				std::shared_ptr<Buffer> buffer = this->GetLastBinBuffer();
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t offset = buffer->GetSize();
				size_t length = sizeof(float)*vec.size();
				buffer->AddBytes((unsigned char*)(&vec[0]), length);
				bufferView->SetByteOffset(offset);
				bufferView->SetByteLength(length);
				bufferView->SetBuffer(buffers_[0]);
				bufferView->SetTarget(GLTF_TARGET_ARRAY_BUFFER);
				bufferViews_.push_back(bufferView);
				return bufferViews_.back();
			}

			const std::shared_ptr<BufferView>& AddBufferView(const std::vector<unsigned int>& vec)
			{
				std::shared_ptr<Buffer> buffer = this->GetLastBinBuffer();
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t offset = buffer->GetSize();
				size_t length = sizeof(unsigned int)*vec.size();
				buffer->AddBytes((unsigned char*)(&vec[0]), length);
				bufferView->SetByteOffset(offset);
				bufferView->SetByteLength(length);
				bufferView->SetBuffer(buffer);
				bufferView->SetTarget(GLTF_TARGET_ELEMENT_ARRAY_BUFFER);
				bufferViews_.push_back(bufferView);
				return bufferViews_.back();
			}

			const std::shared_ptr<BufferView>& AddBufferViewDraco(const std::shared_ptr<Buffer>& buffer, size_t offset = 0)
			{
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t length = (size_t)((int)buffer->GetSize() - (int)offset);
				bufferView->SetByteOffset(offset);
				bufferView->SetByteLength(length);
				bufferView->SetBuffer(buffer);
				bufferView->SetTarget(GLTF_TARGET_ARRAY_BUFFER);
				bufferViews_.push_back(bufferView);
				return bufferViews_.back();
			}

			std::shared_ptr<Buffer> AddBufferDraco(const std::shared_ptr<::kml::Mesh>& mesh, bool is_union_buffer = false)
			{
				std::vector<unsigned char> bytes;
				
				if( !SaveToDraco(bytes, mesh) )
				{
					return std::shared_ptr<Buffer>();
				}

				if (!is_union_buffer)
				{
					dracoBuffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, buffers_.size(), true)));
				}
				else
				{
					if (dracoBuffers_.empty())
					{
						dracoBuffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, buffers_.size(), true, true)));
					}
				}
				std::shared_ptr<Buffer>& buffer = dracoBuffers_.back();
				buffer->AddBytes(&bytes[0], bytes.size());
				return buffer;
			}
		protected:
			std::vector<std::shared_ptr<Node> > nodes_;
			std::vector<std::shared_ptr<Mesh> > meshes_;
			std::vector<std::shared_ptr<Accessor> > accessors_;
			std::vector<std::shared_ptr<BufferView> > bufferViews_;
			std::vector<std::shared_ptr<Buffer> > buffers_;
			std::vector<std::shared_ptr<Buffer> > dracoBuffers_;
			std::string basename_;
		};

		static
		int FindTextureIndex(const std::vector<std::string>& v, const std::string& s)
		{
			std::vector<std::string>::const_iterator it = std::find(v.begin(), v.end(), s);
			if (it != v.end())
			{
				return std::distance(v.begin(), it);
			}
			return -1;
		}

		static
		void RegisterNodes(
			ObjectRegister& reg,
			const std::shared_ptr<::kml::Node>& node,
			bool IsOutputBin,
			bool IsOutputDraco, bool IsUnionBufferDraco)
		{
			auto msh = node->GetMesh();
			if (msh.get())
			{
				if (IsOutputBin)
				{
					reg.RegisterObject(node);
				}
				
				if (IsOutputDraco)
				{
					reg.RegisterObjectDraco(node, IsUnionBufferDraco);
				}
				
			}
	
			{
				auto& children = node->GetChildren();
				for (size_t i = 0; i < children.size(); i++)
				{
					RegisterNodes(reg, children[i], IsOutputBin, IsOutputDraco, IsUnionBufferDraco);
				}
			}
		}

		static
		bool NodeToGLTF(
			picojson::object& root, 
			ObjectRegister& reg, 
			const std::shared_ptr<::kml::Node>& node,
			bool IsOutputBin, 
			bool IsOutputDraco,
			bool IsUnionBufferDraco)
		{
			{
				picojson::object sampler;
				sampler["magFilter"] = picojson::value((double)GLTF_TEXTURE_FILTER_LINEAR);//WebGLConstants.LINEAR
				sampler["minFilter"] = picojson::value((double)GLTF_TEXTURE_FILTER_LINEAR);//WebGLConstants.NEAREST_MIPMAP_LINEAR
				sampler["wrapS"] = picojson::value((double)GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE);
				sampler["wrapT"] = picojson::value((double)GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE);
				picojson::array samplers;
				samplers.push_back(picojson::value(sampler));
				root["samplers"] = picojson::value(samplers);
			}
			typedef std::map<std::string, std::string> CacheMapType;
			std::vector<std::string> texture_vec;
			std::map<std::string, std::string> cache_map;
			{
				std::set<std::string> texture_set;
				GetTextures(texture_set, node->GetMaterials());

				//std::ofstream fff("c:\\src\\debug.txt");

				static const std::string t = "_s0.";
				for (std::set<std::string>::const_iterator it = texture_set.begin(); it != texture_set.end(); ++it)
				{
					std::string tex = *it;
					//fff << tex << std::endl;

					if (tex.find(t) == std::string::npos)
					{	
						texture_vec.push_back(tex);
					}
					else
					{
						std::string orgPath = tex;
						orgPath.replace(tex.find(t), t.size(), ".");
						orgPath = RemoveExt(orgPath);
						cache_map.insert(CacheMapType::value_type(orgPath, tex));
					}
				}
			}
			{
				int level = 0;
				picojson::array images;
				picojson::array textures;
				for (size_t i = 0; i < texture_vec.size(); i++)
				{
					std::string imagePath = texture_vec[i];
					std::string imageId = GetImageID(imagePath);
					std::string texuteId = GetTextureID(imagePath);

					picojson::object image;
					image["name"] = picojson::value(imageId);
					image["uri"] = picojson::value(imagePath);
					{
						CacheMapType::const_iterator it = cache_map.find(RemoveExt(imagePath));
						if (it != cache_map.end())
						{
							//"extensions": {"KSK_preloadUri":{"uri":"Default_baseColor_pre.jpg"}}
							picojson::object extensions;
							picojson::object KSK_preloadUri;
							KSK_preloadUri["uri"] = picojson::value(it->second);
							extensions["KSK_preloadUri"] = picojson::value(KSK_preloadUri);
							image["extensions"] = picojson::value(extensions);
						}
					}
					images.push_back(picojson::value(image));

					picojson::object texture;
					int nFormat = GetImageFormat(imagePath);
					texture["format"] = picojson::value((double)nFormat);         //image.format;
					texture["internalFormat"] = picojson::value((double)nFormat); //image.format;
					texture["sampler"] = picojson::value((double)0);
					texture["source"] = picojson::value((double)i);   //imageId;
					texture["target"] = picojson::value((double)GLTF_TEXTURE_TARGET_TEXTURE2D); //WebGLConstants.TEXTURE_2D;
					texture["type"] = picojson::value((double)GLTF_TEXTURE_TYPE_UNSIGNED_BYTE); //WebGLConstants.UNSIGNED_BYTE

					textures.push_back(picojson::value(texture));
				}
				if (!images.empty())
				{
					root["images"] = picojson::value(images);
				}
				if (!textures.empty())
				{
					root["textures"] = picojson::value(textures);
				}
			}

			{
				RegisterNodes(reg, node, IsOutputBin, IsOutputDraco, IsUnionBufferDraco);
			}


			{
				root["scene"] = picojson::value((double)0);
			}

			{
				picojson::array ar;

				picojson::object scene;
				picojson::array nodes_;
				const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
				for (size_t i = 0; i < nodes.size(); i++)
				{
					nodes_.push_back(picojson::value((double)nodes[i]->GetIndex()));
				}
				scene["nodes"] = picojson::value(nodes_);
				ar.push_back(picojson::value(scene));

				root["scenes"] = picojson::value(ar);
			}

			{
				static const picojson::value matrix_[] = {
					picojson::value(1.0), picojson::value(0.0), picojson::value(0.0), picojson::value(0.0),
					picojson::value(0.0), picojson::value(1.0), picojson::value(0.0), picojson::value(0.0),
					picojson::value(0.0), picojson::value(0.0), picojson::value(1.0), picojson::value(0.0),
					picojson::value(0.0), picojson::value(0.0), picojson::value(0.0), picojson::value(1.0)
				};
				static const picojson::array matrix(matrix_, matrix_ + 16);

				const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
				picojson::array ar;
				for (size_t i = 0; i < nodes.size(); i++)
				{
					const std::shared_ptr<Node>& n = nodes[i];
					picojson::object nd;
					nd["matrix"] = picojson::value(matrix);
					nd["mesh"] = picojson::value((double)n->GetMesh()->GetIndex());//TODO;//picojson::value(node->GetMesh()->GetName());//TODO
					ar.push_back(picojson::value(nd));
				}
				root["nodes"] = picojson::value(ar);
			}

			{
				const std::vector< std::shared_ptr<Mesh> >& meshes = reg.GetMeshes();
				//std::cout << meshes.size() << std::endl;
				picojson::array ar;
				for (size_t i = 0; i < meshes.size(); i++)
				{
					const std::shared_ptr<Mesh>& mesh = meshes[i];
					picojson::object nd;
					nd["name"] = picojson::value(mesh->GetName());

					picojson::object attributes;
					{
						attributes["NORMAL"] = picojson::value((double)mesh->GetAccessor("NORMAL")->GetIndex());//picojson::value(mesh->GetAccessor("NORMAL")->GetName());
						attributes["POSITION"] = picojson::value((double)mesh->GetAccessor("POSITION")->GetIndex());;
						std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
						if (tex.get())
						{
							attributes["TEXCOORD_0"] = picojson::value((double)tex->GetIndex());
						}
					}
					picojson::object primitive;
					primitive["attributes"] = picojson::value(attributes);
					primitive["indices"] = picojson::value((double)mesh->GetIndices()->GetIndex());
					primitive["mode"] = picojson::value((double)mesh->GetMode());
					primitive["material"] = picojson::value((double)mesh->GetMaterialID());

					if (IsOutputDraco)
					{
						const std::vector< std::shared_ptr<BufferView> >& bufferViews = reg.GetBufferViews();
						if (i < bufferViews.size())
						{
							const std::shared_ptr<BufferView>& bufferview = bufferViews[i];
							picojson::object KHR_draco_mesh_compression;
							KHR_draco_mesh_compression["bufferView"] = picojson::value((double)bufferview->GetIndex());//TODO
							if (false)
							{
								//old style
								picojson::array attributesOrder;
								attributesOrder.push_back(picojson::value("POSITION"));
								std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
								if (tex.get())
								{
									attributesOrder.push_back(picojson::value("TEXCOORD_0"));
								}
								attributesOrder.push_back(picojson::value("NORMAL"));
								KHR_draco_mesh_compression["attributesOrder"] = picojson::value(attributesOrder);
								KHR_draco_mesh_compression["version"] = picojson::value("0.9.1");
							}
							else
							{
								//new style
								int nOrder = 0;
								picojson::object attributes;
								attributes["POSITION"] = picojson::value((double)nOrder++);
								std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
								if (tex.get())
								{
									attributes["TEXCOORD_0"] = picojson::value((double)nOrder++);
								}
								attributes["NORMAL"] = picojson::value((double)nOrder++);
								KHR_draco_mesh_compression["attributes"] = picojson::value(attributes);
							}

							picojson::object extensions;
							extensions["KHR_draco_mesh_compression"] = picojson::value(KHR_draco_mesh_compression);
							primitive["extensions"] = picojson::value(extensions);
						}
					}


					picojson::array primitives;
					primitives.push_back(picojson::value(primitive));

					nd["primitives"] = picojson::value(primitives);
					ar.push_back(picojson::value(nd));
				}
				root["meshes"] = picojson::value(ar);
			}

			{//
				const std::vector< std::shared_ptr<Accessor> >& accessors = reg.GetAccessors();
				picojson::array ar;
				for (size_t i = 0; i < accessors.size(); i++)
				{
					const std::shared_ptr<Accessor>& accessor = accessors[i];
					picojson::object nd;
					//nd["name"] = picojson::value(accessor->GetName());
					std::shared_ptr<BufferView> bufferView = accessor->GetBufferView();
					if (bufferView.get())
					{
						nd["bufferView"] = picojson::value((double)bufferView->GetIndex());
					}
					nd["byteOffset"] = accessor->Get("byteOffset");
					//nd["byteStride"] = accessor->Get("byteStride");
					nd["componentType"] = accessor->Get("componentType");
					nd["count"] = accessor->Get("count");
					nd["type"] = accessor->Get("type");
					nd["min"] = accessor->Get("min");
					nd["max"] = accessor->Get("max");
					ar.push_back(picojson::value(nd));
				}
				root["accessors"] = picojson::value(ar);
			}

			{//buffer view
				const std::vector< std::shared_ptr<BufferView> >& bufferViews = reg.GetBufferViews();
				picojson::array ar;
				for (size_t i = 0; i < bufferViews.size(); i++)
				{
					const std::shared_ptr<BufferView>& bufferView = bufferViews[i];
					picojson::object nd;
					//nd["name"] = picojson::value(bufferView->GetName());
					nd["buffer"] = picojson::value((double)bufferView->GetBuffer()->GetIndex());
					nd["byteOffset"] = picojson::value((double)bufferView->GetByteOffset());
					nd["byteLength"] = picojson::value((double)bufferView->GetByteLength());
					nd["target"] = picojson::value((double)bufferView->GetTarget());
					ar.push_back(picojson::value(nd));
				}
				root["bufferViews"] = picojson::value(ar);
			}

			{
				picojson::array ar;
				if (IsOutputBin)
				{
					const std::vector< std::shared_ptr<Buffer> >& buffers = reg.GetBuffers();
					for (size_t i = 0; i < buffers.size(); i++)
					{
						const std::shared_ptr<Buffer>& buffer = buffers[i];
						picojson::object nd;
						//nd["name"] = picojson::value(buffer->GetName());
						nd["byteLength"] = picojson::value((double)buffer->GetByteLength());
						nd["uri"] = picojson::value(buffer->GetURI());
						ar.push_back(picojson::value(nd));
					}
				}
				if (IsOutputDraco)
				{
					const std::vector< std::shared_ptr<Buffer> >& buffers = reg.GetBuffersDraco();
					for (size_t i = 0; i < buffers.size(); i++)
					{
						const std::shared_ptr<Buffer>& buffer = buffers[i];
						picojson::object nd;
						//nd["name"] = picojson::value(buffer->GetName());
						nd["byteLength"] = picojson::value((double)buffer->GetByteLength());
						nd["uri"] = picojson::value(buffer->GetURI());
						ar.push_back(picojson::value(nd));
					}
				}
				root["buffers"] = picojson::value(ar);
			}

			{
				const auto& materials = node->GetMaterials();
				picojson::array ar;
				for (size_t i = 0; i < materials.size(); i++)
				{
					const auto& mat = materials[i];
					picojson::object nd;
					nd["name"] = picojson::value(mat->GetName());
					picojson::array emissiveFactor;
					emissiveFactor.push_back(picojson::value(0.0));
					emissiveFactor.push_back(picojson::value(0.0));
					emissiveFactor.push_back(picojson::value(0.0));
					nd["emissiveFactor"] = picojson::value(emissiveFactor);

					picojson::object pbrMetallicRoughness;

					std::string diffuse_texname = mat->GetTextureName("Diffuse");
					if (!diffuse_texname.empty())
					{
						int nIndex = FindTextureIndex(texture_vec, diffuse_texname);
						if (nIndex >= 0)
						{
							picojson::object baseColorTexture;
							baseColorTexture["index"] = picojson::value((double)nIndex);
							pbrMetallicRoughness["baseColorTexture"] = picojson::value(baseColorTexture);
						}
					}
					/*
					std::string diffuse_cache_texname = mat->GetTextureName("DiffuseS0");
					if (!diffuse_cache_texname.empty())
					{
						int nIndex = FindTextureIndex(texture_vec, diffuse_cache_texname);
						if (nIndex >= 0)
						{
							picojson::object baseColorTexture;
							baseColorTexture["index"] = picojson::value((double)nIndex);
							pbrMetallicRoughness["baseColorTexture"] = picojson::value(baseColorTexture);
						}
					}
					*/

					std::string normal_texname = mat->GetTextureName("Normal");
					if (!normal_texname.empty())
					{
						int nIndex = FindTextureIndex(texture_vec, normal_texname);
						if (nIndex >= 0)
						{
							picojson::object normalTexture;
							normalTexture["index"] = picojson::value((double)nIndex);
							nd["normalTexture"] = picojson::value(normalTexture);
						}
					}


					picojson::array colorFactor;
					float R = mat->GetValue("Diffuse.R");
					float G = mat->GetValue("Diffuse.G");
					float B = mat->GetValue("Diffuse.B");
					float A = mat->GetValue("Diffuse.A");

					colorFactor.push_back(picojson::value(R));
					colorFactor.push_back(picojson::value(G));
					colorFactor.push_back(picojson::value(B));
					colorFactor.push_back(picojson::value(A));
					pbrMetallicRoughness["baseColorFactor"] = picojson::value(colorFactor);

					pbrMetallicRoughness["metallicFactor"] = picojson::value(mat->GetFloat("metallicFactor"));
					pbrMetallicRoughness["roughnessFactor"] = picojson::value(mat->GetFloat("roughnessFactor"));
					nd["pbrMetallicRoughness"] = picojson::value(pbrMetallicRoughness);

					if (A >= 1.0f)
					{
						nd["alphaMode"] = picojson::value("OPAQUE");
					}
					else
					{
						nd["alphaMode"] = picojson::value("BLEND");
					}

					ar.push_back(picojson::value(nd));
				}
				root["materials"] = picojson::value(ar);
			}

			return true;
		}
	}
	//-----------------------------------------------------------------------------

	static
	bool ExportGLTF(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts, bool prettify = true)
	{
		bool output_bin   = true;
		bool output_draco = true;
		bool union_buffer_draco = true;
		
		//std::shared_ptr<Options> opts = Options::GetGlobalOptions();
		int output_buffer = opts->GetInt("output_buffer");
		if (output_buffer == 0)
		{
			output_bin = true;
			output_draco = false;
			union_buffer_draco = false;
		}
		else if (output_buffer == 1)
		{
			output_bin = false;
			output_draco = true;
		}
		else
		{
			output_bin = true;
			output_draco = true;
		}
		
		bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;

		std::string base_dir  = GetBaseDir(path);
		std::string base_name = GetBaseName(path);
		gltf::ObjectRegister reg(base_name);
		picojson::object root_object;

		{
			picojson::object asset;
			asset["generator"] = picojson::value("glTF-Maya-Exporter");
			asset["version"] = picojson::value("2.0");
			root_object["asset"] = picojson::value(asset);
		}

		{
			picojson::array extensionsUsed;
			picojson::array extensionsRequired;
			if (output_draco)
			{
				extensionsUsed.push_back(picojson::value("KHR_draco_mesh_compression"));
				extensionsRequired.push_back(picojson::value("KHR_draco_mesh_compression"));
			}
			if (make_preload_texture)
			{
				extensionsUsed.push_back(picojson::value("KSK_preloadUri"));
				extensionsRequired.push_back(picojson::value("KSK_preloadUri"));
			}
			root_object["extensionsUsed"] = picojson::value(extensionsUsed);
			root_object["extensionsRequired"] = picojson::value(extensionsRequired);
		}


		if (!gltf::NodeToGLTF(root_object, reg, node, output_bin, output_draco, union_buffer_draco))
		{
			return false;
		}

		{
			std::ofstream ofs(path.c_str());
			if (!ofs)
			{
				std::cerr << "Could't write outputfile" << std::endl;
				return false;
			}

			picojson::value(root_object).serialize(std::ostream_iterator<char>(ofs), prettify);
		}

		if(output_bin)
		{
			const std::shared_ptr<kml::gltf::Buffer>& buffer = reg.GetBuffers()[0];
			if (buffer->GetSize() > 0)
			{
				std::string binfile = base_dir + buffer->GetURI();
				std::ofstream ofs(binfile.c_str(), std::ofstream::binary);
				if (!ofs)
				{
					std::cerr << "Could't write outputfile" << std::endl;
					return false;
				}
				ofs.write((const char*)buffer->GetBytesPtr(), buffer->GetByteLength());
			}
		}
		
		if (output_draco)
		{
			const std::vector< std::shared_ptr<kml::gltf::Buffer> >& buffers = reg.GetBuffersDraco();
			//std::cerr << "Draco Buffer Size"<< buffers.size() << std::endl;
			for (size_t j = 0; j < buffers.size(); j++)
			{
				const std::shared_ptr<kml::gltf::Buffer>& buffer = buffers[j];
				std::string binfile = base_dir + buffer->GetURI();
				std::ofstream ofs(binfile.c_str(), std::ofstream::binary);
				if (!ofs)
				{
					std::cerr << "Could't write outputfile" << std::endl;
					return -1;
				}
				ofs.write((const char*)buffer->GetBytesPtr(), buffer->GetByteLength());
			}
		}

		return true;
	}


	bool glTFExporter::Export(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts)const
	{
		return ExportGLTF(path, node, opts);
	}
}