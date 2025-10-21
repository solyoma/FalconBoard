#pragma once
#ifndef _QUADTREE_H
	#define _QUADTREE_H

// Based on https://github.com/pvigier/Quadtree
// but using Qt types
// interesting: QArea in pvigier has exactly the
// same member functions as QRectF, except the case of
// the function names

/*----------------------------------------------------------
* Quadtree implementation for drawable and visible history items.
*	Only pointers to visible history elements are stored in
*	this quad tree. Pointers stored here are ordered by z-order
*	it must be done before display
* Algorithm:
*	We start with an undivided area of one full screen. 
*	This will be the whole 'paper' area and the 'actual' 
* 	area we try to insert visible items into.
* 
*	Every time a history element becomes visible first it is checked
*	if its area fits into the 'paper' area.
*	If not, then the 'paper' area is extended to contain the new item
*	and the partitioning is recalculated. 
*	The 'paper' area never shrinks. If a history item is deleted 
* 	or hidden the 'paper' actual area remains the same, but some 
*	nodes in the quadtree may be merged.
* 
*	If the number of items in the actual area into which the new
*	item is added is larger than '_maxAllowed' and the '_maxLevel'
*	level is not yet reached the area is sub-divided to quadrants 
*	and the new item is inserted into the actual area and 
*	the corresponding sub area(s). 
*	'_maxLevel' is recalculated when the 'paper' area changes, so 
*	that the smallest sub area possible is _MIN_WIDTH x _MIN_HEIGHT
*	pixels.
* 
* To make this more general no QT functions are used here, only 
* STL.
* 
* How to use:
*	In class History add two functions. 
* 		const QuadArea AreaForItem(int &)
*	to get the area of an item and
*		const bool AreEquals(HistoryItem &*p1, HistoryItem &*p2)
*	to determine equality of items:
* 
*	In each History() constructor create a QuadTree for  
*		the actual screen area and ads these functions to them
*	If a file is loaded into this history then after all is loaded
*	add this to the Quad tree using the
*	Also add items when the user adds a screenshot or a scribble.
*	if the area of the new item lies outside the existing area
* 
*	For display collect the pointers to the drawables into
*   an std::vector using GetValues(visible_area) and display only them
*/

#include <array>
#include <memory>
#include <functional>
#if !defined _VIEWER && defined _DEBUG
	#include <iostream>
	#include <fstream>
#endif

using real = qreal;

using QuadArea = QRectF;

struct QuadTreeDelegeate;	// A. Solyom 2025.05.03 in drawables.h
/*=============================================================
 * When constructing a value type and two functions are required.
 *  'getArea(const &T' must return the area of a given value and
 *  'isEqual(const &T, const&T)' must check if the values are equal
 * 
 * A QuadTree has this as its head and a series of linked nodes
 *  'invoke_result_t' requires C++17
 *------------------------------------------------------------*/
template <typename T, typename AreaFor, typename Equal = std::equal_to<T>> 
class QuadTree
{
	static_assert(std::is_convertible_v<std::invoke_result_t<AreaFor, const T&>, QuadArea>,
		"AreaFor must be a callable of signature QuadArea(const T&)");
	static_assert(std::is_convertible_v<std::invoke_result_t<Equal, const T&, const T&>, bool>,
		"Equal must be a callable of signature bool(const T&, const T&)");

	friend struct QuadTreeDelegate;		// A. Solyom 2025.05.3
public:
	const int _MIN_WIDTH = 200, _MIN_HEIGHT = 200;
	const size_t _MAX_ALLOWED_IN_ONE_NODE = 32; // if more than this many items and _maxDepth not reached 
											 // and the item's area fits into the children => split node to 4 children

	QuadTree(QuadArea area, AreaFor areaFor, Equal isEqual) : _area(area), _AreaFor(areaFor), _Equal(isEqual), _rootNode(std::make_unique<QuadNode>())
	{
			_CalcMaxDepth(_area);
	}

	void Add(const T& value)	// dynamically change the area
	{
		bool res = false;
		QuadArea areaV = _AreaFor(value);
		if (!_area.contains(areaV))
		{				  // area always starts at (0,0)
			QuadArea newArea = _area.united(areaV);
				// to decrease the frequency of resizing enlarge area minimum 1000 points in any direction
			if (newArea.width() - _area.width())  // then enlarge x
				newArea.setWidth(newArea.width() + 1000);
			if (newArea.height() - _area.height())  // then enlarge y
				newArea.setHeight(newArea.height() + 1000);
			Resize(newArea, &value, true); // also adds new value
		}
		else
			res = _Add(_rootNode.get(), 0, _area, value);
		if(res)
			++_count;
	}

	void Remove(const T& value)
	{
		_Remove(_rootNode.get(), _area, value);
		if (_count) 
			--_count;
	}

	std::vector<T> GetValues(const QuadArea& area) const
	{
		auto values = std::vector<T>();
		if(_area.intersects(area) )
			_Query(_rootNode.get(), _area, area, values);
		return values;
	}

	void Resize(QuadArea area, const T* pNewItem = nullptr, bool savePrevVals=true)
	{
		std::vector<T> values;
		if(savePrevVals)
			values = GetValues(_area);

		if (pNewItem)
			values.push_back(*pNewItem);

		_rootNode.get()->Clear();
		_count = 0;
		_area = area;
		_CalcMaxDepth(_area);
		_AddAllItems(values);
	}

	constexpr QuadArea Area() const
	{
		return _area;
	}

	constexpr std::size_t Count(QuadArea area = QuadArea()) 
	{
		if (area.isValid())
			return _CountForArea(_rootNode.get(), _area, area);
		else
			return _count;
	}

	constexpr T BottomItem() 
	{
		int y = 0;
		T item = 0;
		_BottomItem(_rootNode.get(), item, y);
		return item;
	}
#if !defined _VIEWER && defined _DEBUG
	constexpr void DebugPrint()
	{
		std::ofstream f;
		f.open("quadtreeDebug.txt");
		f << "Root Node. Total count: " << _count << ", area:"; 
		_DebugAreaPrint(f, _area);
		f << " contains " << _rootNode->values.size() << " values\n";
		if (!_IsLeaf(_rootNode.get()))
			for (size_t i = 0; i < _rootNode->children.size(); ++i)
				_DebugPrint(f, _rootNode->children[i].get(), _area, i, "    ");
		f.close();
	}
#endif
private:
			// these two functions defined elsewhere
	AreaFor *_AreaFor;
	Equal *_Equal;

	// contains pointers to children and an array of items (pointers to HistoryItems) inside this node
	// only values are added here which are completely inside this node
	struct QuadNode		
	{
		std::array<std::unique_ptr<QuadNode>, 4> children;
		std::vector<T> values;

		void Clear()
		{
			for (auto &ch : children)
			{
				if (ch.get())
				{
					ch.get()->Clear();
					ch.reset();
				}
			}
			values.clear();
		}
	};

	std::unique_ptr<QuadNode> _rootNode;	// pointer so it can be deleted

	QuadArea _area;		// for this level
	int _maxDepth = 10;
	std::size_t _count = 0;

	void _CalcMaxDepth(QuadArea area)
	{
		QSizeF size = area.size();
		_maxDepth = 1;
		while (size.width() > _MIN_WIDTH && size.height() > _MIN_HEIGHT)
		{
			++_maxDepth;
			size /= real(2);
		}
	}

	bool _IsLeaf(const QuadNode* node) const	// either all children are nullptr or none of them
	{
		return !static_cast<bool>(node->children[0]);
	}

	QuadArea _AreaOfThisChild(const QuadArea& areaOfParent, int quadrant) const
	{
		auto origin = areaOfParent.topLeft();
		auto childSize = areaOfParent.size() / static_cast<real>(2);
		switch (quadrant)
		{
				// North West
			case 0:
				return QuadArea(origin, childSize);
				// Norst East
			case 1:
				return QuadArea(QPointF(origin.x() + childSize.width(), origin.y()), childSize);
				// South West
			case 2:
				return QuadArea(QPointF(origin.x(), origin.y() + childSize.height()), childSize);
				// South East
			case 3:
				return QuadArea(QPointF(origin.x() + childSize.width(), origin.y() + childSize.height() ), childSize);
			default:
				assert(false && "Invalid child index");
				return QuadArea();
		}
	}

	int _GetQuadrant(const QuadArea& nodeArea, const QuadArea& valueArea) const // must be fully inside quadrant!
	{
		auto center = nodeArea.center();
		// West
		if (valueArea.right() < center.x())
		{
			// North West
			if (valueArea.bottom() < center.y())
				return 0;
			// South West
			else if (valueArea.top() >= center.y())
				return 2;
			// Not contained in any quadrant
			else
				return -1;
		}
		// East
		else if (valueArea.left() >= center.x())
		{
			// North East
			if (valueArea.bottom() < center.y())
				return 1;
			// South East
			else if (valueArea.top() >= center.y())
				return 3;
			// Not contained in any quadrant
			else
				return -1;
		}
		// Not contained in any quadrant
		else
			return -1;
	}

	bool _Add(QuadNode* node, std::size_t depth, const QuadArea& area, const T& value)
	{
		assert(node != nullptr);
		QuadArea areaV = _AreaFor(value);
		assert(area.contains(areaV));
		if (std::find(node->values.begin(), node->values.end(), value) != node->values.end())		// already added
			return false;

		bool res = false;

		if (_IsLeaf(node))
		{

			bool insertIntoThisNode =
				(node->values.size() < _MAX_ALLOWED_IN_ONE_NODE) ||	   // when there is space
				(depth == (size_t)_maxDepth); // ||								   // or no more split possible
				// (childAreaSize.x <= areaV.Width() || childAreaSize.y <= areaV.Height()); // or does not fit into any children
			
			if (insertIntoThisNode)
			{
				node->values.push_back(value);
				res = true;
			}
			else  // Otherwise, we split and we try again
			{
				_Split(node, area);
				res = _Add(node, depth, area, value);	// still may not be added to any children
			}
		}
		else
		{
			auto i = _GetQuadrant(area, areaV);
			// Add the value to a child if the value is entirely contained in it
			if (i != -1)
				res = _Add(node->children[static_cast<std::size_t>(i)].get(), depth + 1, _AreaOfThisChild(area, i), value);
			else	   // Otherwise, we add the value to the current node
			{
				node->values.push_back(value);
				res = true;
			}
		}
		return res;
	}

	void _Split(QuadNode* node, const QuadArea& area)
	{
		assert(node != nullptr);
		assert(_IsLeaf(node) && "Only leaves can be split");
		// Create children
		node->children[0] = std::make_unique<QuadNode>();
		node->children[1] = std::make_unique<QuadNode>();
		node->children[2] = std::make_unique<QuadNode>();
		node->children[3] = std::make_unique<QuadNode>();
		// Move those values to children from parent
		// whose area is completely inside one child
		auto valuesForParent = std::vector<T>();
		for (const auto& value : node->values)
		{
			auto i = _GetQuadrant(area, _AreaFor(value));
			if (i != -1)
				node->children[static_cast<std::size_t>(i)]->values.push_back(value);
			else
				valuesForParent.push_back(value);		// this value remains in node->values
		}
		node->values = std::move(valuesForParent);
	}

	bool _Remove(QuadNode* node, const QuadArea& area, const T& value)
	{
		assert(node != nullptr);
		assert(area.contains(_AreaFor(value)));
		if (_IsLeaf(node))
		{
			// Remove the value from node
			_RemoveValue(node, value);
			return true;
		}
		else
		{
			// Remove the value in a child if the value is entirely contained in it
			auto i = _GetQuadrant(area, _AreaFor(value));
			if (i != -1)
			{
				if (_Remove(node->children[static_cast<std::size_t>(i)].get(), _AreaOfThisChild(area, i), value))
					return _TryMerge(node);
			}
			// Otherwise, we remove the value from the current node
			else
				_RemoveValue(node, value);
			return false;
		}
	}

	void _RemoveValue(QuadNode* node, const T& value)
	{
		// Find the value in node->values
		auto it = std::find_if(std::begin(node->values), std::end(node->values),
			[this, &value](const auto& rhs) { return _Equal(value, rhs); });
		//assert(it != std::end(node->values) && "Trying to remove a value that is not present in the node");
		if (it != node->values.end())
		{// Swap with the last element and pop back
			*it = std::move(node->values.back());
			node->values.pop_back();
		}
	}

	bool _TryMerge(QuadNode* node)
	{
		assert(node != nullptr);
		assert(!_IsLeaf(node) && "Only interior nodes can be merged");
		auto nbValues = node->values.size();
		for (const auto& child : node->children)
		{
			if (!_IsLeaf(child.get()))
				return false;
			nbValues += child->values.size();
		}
		if (nbValues <= _MAX_ALLOWED_IN_ONE_NODE)
		{
			node->values.reserve(nbValues);
			// Merge the values of all the children
			for (const auto& child : node->children)
			{
				for (const auto& value : child->values)
					node->values.push_back(value);
			}
			// Remove the children
			for (auto& child : node->children)
				child.reset();
			return true;
		}
		else
			return false;
	}

	void _Query(QuadNode* node, const QuadArea& area, const QuadArea& queryArea, std::vector<T>& values) const
	{
		assert(node != nullptr);
		assert(queryArea.intersects(area));
		for (const auto& value : node->values)
		{
			if (queryArea.intersects(_AreaFor(value)))
				values.push_back(value);
		}
		if (!_IsLeaf(node))
		{
			for (auto i = std::size_t(0); i < node->children.size(); ++i)
			{
				auto childArea = _AreaOfThisChild(area, static_cast<int>(i));
				if (queryArea.intersects(childArea))
					_Query(node->children[i].get(), childArea, queryArea, values);
			}
		}
	}

	int _CountForArea(QuadNode *node, const QuadArea area, const QuadArea queryArea)
	{
		int cnt = 0;
		assert(area.isValid());
		for (const auto& value : node->values)
		{
			if (queryArea.intersects(_AreaFor(value)))
				++cnt;
		}
		if (!_IsLeaf(node))
		{
			for (auto i = std::size_t(0); i < node->children.size(); ++i)
			{
				auto childArea = _AreaOfThisChild(area, static_cast<int>(i));
				if (queryArea.intersects(childArea))
					cnt += _CountForArea(node->children[i].get(), childArea, queryArea);
			}
		}
		return cnt;
	}

	// SA

	void _AddAllItems(std::vector<T> &values)
	{
		for (auto v : values)
			Add(v);
	}

	int _BottomItem(QuadNode* node, T &item, int _bottomY) // returns y for _bottom item + item in item
	{
		for (auto v : node->values)
		{
			QuadArea av = _AreaFor(v);
			if (av.bottom() > _bottomY)
			{
				item = v;
				_bottomY = av.bottom();
			}
		}

		if (!_IsLeaf(node))
		{
			_bottomY = _BottomItem(node->children[0].get(), item, _bottomY);
			_bottomY = _BottomItem(node->children[1].get(), item, _bottomY);
			_bottomY = _BottomItem(node->children[2].get(), item, _bottomY);
			_bottomY = _BottomItem(node->children[3].get(), item, _bottomY);
		}
		return _bottomY;
	}
#if !defined _VIEWER && defined _DEBUG
	void _DebugAreaPrint(std::ofstream &f, QuadArea area)
	{
		f << "(" << area.left() << "," << area.top() << " " << area.width() << " x " << area.height() << ") ";
	}
	void _DebugPrint(std::ofstream& f, QuadNode* node, QuadArea areaOfParent, int quadrant, std::string indent)
	{
		QuadArea area = _AreaOfThisChild(areaOfParent, quadrant);
		f << indent << "Level #"<< indent.length()/4 <<" Child node - area:";
		_DebugAreaPrint(f, area);
		f << " contains " << node->values.size() << " values\n";
		int val = 947;
		if (std::find(std::begin(node->values),std::end(node->values), val) != std::end(node->values))
			f << indent << indent << " contains 947\n";
		if (!_IsLeaf(node))
			for (size_t i = 0; i < node->children.size(); ++i)
				_DebugPrint(f, node->children[i].get(), area, i, indent + "   ");
	}
#endif
};


#endif
