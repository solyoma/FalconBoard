#pragma once
#ifndef _QUADTREE_H
	#define _QUADTREE_H

// Based on https://github.com/pvigier/Quadtree

/*----------------------------------------------------------
* Quadtree implementation for drawable and visible history items.
*	Only pointers to visible hystory elements are stored in
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
*	In class History add  to get area of item
* 		const QuadArea AreaForItem(int &)
*	and determine equality of items:
*		const bool AreEquals(HistoryItem &*p1, HistoryItem &*p2)
* 
*	In each History() constructor create a QuadTree for  
*		the actual screen area and ading them these functions
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

using real = float;

struct V2
{
	real x, y;
	constexpr V2(real x, real y) :x(x), y(y) {};
	constexpr V2 operator+=(const V2 o) noexcept { x += o.x; y += o.y; return *this; }
	constexpr V2 operator/=(real c)		noexcept { x /= c; y /= c; return *this; }
};
constexpr V2 operator+(const V2 l, const V2 o) noexcept { return V2(l.x + o.x, l.y + o.y); }
constexpr V2 operator/(const V2 l, real c) { return V2(l.x / c, l.y / c); }

class QuadArea
{
	real _left=0, _top=0, _right=0, _bottom=0;
public:
	QuadArea() {}
	QuadArea(real left, real top, real width, real height) noexcept :_left(left), _top(top), _right(left + width), _bottom(_top + height) 
	{
		assert(_right > _left && _bottom > _top);
	};
	QuadArea(V2 leftTop, V2 size) noexcept : QuadArea(leftTop.x, leftTop.y, size.x, size.y) 
	{
		assert(size.x && size.y);
	}
	bool IsValid() const { return _left < _right&& _top < _bottom; }

	constexpr bool Contains(const V2& p) const noexcept
	{
		return !(p.x < _left || p.y < _top || p.x > _right || p.y > _bottom); // was >= _right/_bottom
	}

	constexpr bool Contains(const QuadArea& r) const noexcept
	{
		return (r._left >= _left) && (r._right < _right) && (r._top >= _top) && (r._bottom <= _bottom);
	}

	constexpr bool Intersects(const QuadArea& r) const noexcept
	{
		return (_left < r._right && _right >= r._left && _top < r._bottom && _bottom>= r._top);
	}

	QuadArea Union(const QuadArea& other) const noexcept
	{
		QuadArea area(_left,_top,_right,_bottom);
		if (_left > other._left)
			area._left = other._left;
		if (_top > other._top)
			area._top = other._top;
		if (area._right < other._right)
			area._right = other._right;
		if (_bottom < other._bottom)
			area._bottom = other._bottom;

		return area;
	}
	constexpr real Left() const noexcept { return _left; }
	constexpr real Top() const noexcept { return _top; }
	constexpr real Right() const noexcept { return _right; }
	constexpr real Bottom() const noexcept { return _bottom; }
	constexpr real Width() const noexcept { return _right - _left; }
	constexpr real Height() const noexcept { return _bottom - _top; }

	constexpr V2 TopLeft() const noexcept { return V2(_left,_top); }
	constexpr V2 Size() const noexcept { return V2(Width(), Height()); }
	constexpr V2 Center() const noexcept { return V2(_left + Width()/2, _top + Height()/2); }

	constexpr void SetWidth(real newWidth) { _right = _left + newWidth; }
	constexpr void SetHeight(real newHeight) { _bottom = _top + newHeight; }
	
};

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

public:
	const int _MIN_WIDTH = 200, _MIN_HEIGHT = 200;
	const int _MAX_ALLOWED_IN_ONE_NODE = 32; // if more than this many items and _maxDepth not reached 
											 // and the item's area fits into the children => split node to 4 children

	QuadTree(QuadArea area, AreaFor areaFor, Equal isEqual) : _area(area), _AreaFor(areaFor), _Equal(isEqual), _rootNode(std::make_unique<QuadNode>())
	{
			_CalcMaxDepth(_area);
	}

	void Add(const T& value)	// dynamically change the area
	{
		bool res = false;
		QuadArea areaV = _AreaFor(value);
		if (!_area.Contains(areaV))
		{				  // area always starts at (0,0)
			QuadArea newArea = _area.Union(areaV);
				// to decrease the frequency of resizing enlarge area minimum 1000 points in any direction
			if (newArea.Width() - _area.Width())  // then enlarge x
				newArea.SetWidth(newArea.Width() + 1000);
			if (newArea.Height() - _area.Height())  // then enlarge y
				newArea.SetHeight(newArea.Height() + 1000);
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
		if(_area.Intersects(area) )
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
		if (area.IsValid())
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
			for (int i = 0; i < _rootNode->children.size(); ++i)
				_DebugPrint(f, _rootNode->children[i].get(), _area, i, "    ");
		f.close();
	}
#endif
private:
			// these two functions defined elsewhere
	AreaFor *_AreaFor;
	Equal *_Equal;

	// contains pointers to children and an array of items (pointers to HistoryItems) inside this node
	// only values are added here which completely inside this node
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
		V2 size = area.Size();
		_maxDepth = 1;
		while (size.x > _MIN_WIDTH && size.y > _MIN_HEIGHT)
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
		auto origin = areaOfParent.TopLeft();
		auto childSize = areaOfParent.Size() / static_cast<real>(2);
		switch (quadrant)
		{
				// North West
			case 0:
				return QuadArea(origin, childSize);
				// Norst East
			case 1:
				return QuadArea(V2(origin.x + childSize.x, origin.y), childSize);
				// South West
			case 2:
				return QuadArea(V2(origin.x, origin.y + childSize.y), childSize);
				// South East
			case 3:
				return QuadArea(origin + childSize, childSize);
			default:
				assert(false && "Invalid child index");
				return QuadArea();
		}
	}

	int _GetQuadrant(const QuadArea& nodeArea, const QuadArea& valueArea) const // must be fully inside quadrant!
	{
		auto center = nodeArea.Center();
		// West
		if (valueArea.Right() < center.x)
		{
			// North West
			if (valueArea.Bottom() < center.y)
				return 0;
			// South West
			else if (valueArea.Top() >= center.y)
				return 2;
			// Not contained in any quadrant
			else
				return -1;
		}
		// East
		else if (valueArea.Left() >= center.x)
		{
			// North East
			if (valueArea.Bottom() < center.y)
				return 1;
			// South East
			else if (valueArea.Top() >= center.y)
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
		assert(area.Contains(areaV));
		if (std::find(node->values.begin(), node->values.end(), value) != node->values.end())		// already added
			return false;

		bool res = false;

		if (_IsLeaf(node))
		{

			bool insertIntoThisNode =
				(node->values.size() < _MAX_ALLOWED_IN_ONE_NODE) ||	   // when there is space
				(depth == _maxDepth); // ||								   // or no more split possible
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
		assert(area.Contains(_AreaFor(value)));
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
		assert(queryArea.Intersects(area));
		for (const auto& value : node->values)
		{
			if (queryArea.Intersects(_AreaFor(value)))
				values.push_back(value);
		}
		if (!_IsLeaf(node))
		{
			for (auto i = std::size_t(0); i < node->children.size(); ++i)
			{
				auto childArea = _AreaOfThisChild(area, static_cast<int>(i));
				if (queryArea.Intersects(childArea))
					_Query(node->children[i].get(), childArea, queryArea, values);
			}
		}
	}

	int _CountForArea(QuadNode *node, const QuadArea area, const QuadArea queryArea)
	{
		int cnt = 0;
		assert(area.IsValid());
		for (const auto& value : node->values)
		{
			if (queryArea.Intersects(_AreaFor(value)))
				++cnt;
		}
		if (!_IsLeaf(node))
		{
			for (auto i = std::size_t(0); i < node->children.size(); ++i)
			{
				auto childArea = _AreaOfThisChild(area, static_cast<int>(i));
				if (queryArea.Intersects(childArea))
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
			if (av.Bottom() > _bottomY)
			{
				item = v;
				_bottomY = av.Bottom();
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
		f << "(" << area.Left() << "," << area.Top() << " " << area.Width() << " x " << area.Height() << ") ";
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
			for (int i = 0; i < node->children.size(); ++i)
				_DebugPrint(f, node->children[i].get(), area, i, indent + "   ");
	}
#endif
};


#endif
