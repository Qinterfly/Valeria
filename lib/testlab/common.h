#pragma once

#include <string>
#include <vector>
#include <array>

namespace Testlab
{
	enum class Direction
	{
		kNone,
		kX,
		kY,
		kZ
	};

	enum class ResponseType
	{
		kNone,
		kDisp,
		kVeloc,
		kAccel,
		kForce
	};

	struct ResponsePoint
	{
		std::wstring name;
		std::wstring node;
		std::wstring component;
		Direction direction;
		int sign;
	};

	struct ResponseUnit
	{
		int length;
		int mass;
		int time;
		double scale;
		std::wstring name;
	};

	struct ResponseHeader
	{
		ResponseType type;
		std::wstring path;
		std::wstring originalRun;
		std::wstring name;
		ResponsePoint point;
		ResponsePoint refPoint;
		ResponseUnit unit;
		int channel;
		int numAverages;
		std::wstring dimension;
		std::wstring transducer;
		std::wstring functionClass;
		std::wstring comment;
	};

	struct Response
	{
		// Data
		std::vector<double> keys;
		std::vector<double> realValues;
		std::vector<double> imagValues;
		std::vector<std::wstring> labels;

		// Header
		ResponseHeader header;
	};

	struct Node
	{
		std::wstring name;
		std::vector<double> coordinates;
		std::vector<double> angles;
	};

	struct Component
	{
		std::wstring name;
		std::vector<double> coordinates;
		std::vector<double> angles;
		std::vector<Node> nodes;
		std::vector<std::vector<int>> lines;
		std::vector<std::vector<int>> trias;
		std::vector<std::vector<int>> quads;
	};

	struct Dependency
	{
		std::wstring slave;
		std::vector<std::wstring> masters;
		std::vector<int> flags;
	};

	struct Geometry
	{
		std::vector<Component> components;
		std::vector<Dependency> dependencies;
	};

	class IProject
	{
	public:
		virtual ~IProject() = default;

		virtual bool isValid() const = 0;
		virtual std::wstring getPath() = 0;

		// Section
		virtual std::wstring getActiveSection() = 0;
		virtual void createSection(std::wstring const& section, bool isSelect) = 0;
		virtual bool isSectionExist(std::wstring const& section) = 0;

		// Folder
		virtual void createFolder(std::wstring const& section, std::wstring const& folder) = 0;
		virtual bool isFolderExist(std::wstring const& section, std::wstring const& folder) = 0;

		// Responses
		virtual std::vector<Response> getResponses(std::vector<std::wstring> const& paths) = 0;
		virtual std::vector<Response> getSelectedResponses() = 0;
		virtual bool addResponses(std::vector<Response> const& responses, std::wstring const& path) = 0;

		// Geometry
		virtual Geometry getGeometry() = 0;
	};
}

