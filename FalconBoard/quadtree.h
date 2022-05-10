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
	real x1=0, y1=0, x2=0, y2=0;
public:
	QuadArea() {}
	QuadArea(real left, real top, real width, real height) noexcept :x1(left), y1(top), x2(left + width), y2(top + height) 
	{
		assert(x2 > x1 && y2 > y1);
	};
	QuadArea(V2 leftTop, V2 size) noexcept : QuadArea(leftTop.x, leftTop.y, size.x, size.y) 
	{
		assert(size.x && size.y);
	}
	bool IsValid() const { return x1 < x2&& y1 < y2; }

	constexpr bool Contains(const V2& p) const noexcept
	{
		return !(p.x < x1 || p.y < y1 || p.x > x2 || p.y > y2); // was >= x2/y2
	}

	constexpr bool Contains(const QuadArea& r) const noexcept
	{
		return (r.x1 >= x1) && (r.x2 < x2) && (r.y1 >= y1) && (r.y2 <= y2);
	}

	constexpr bool Intersects(const QuadArea& r) const noexcept
	{
		return (x1 < r.x2 && x2 >= r.x1 && y1 < r.y2 && y2>= r.y1);
	}

	QuadArea Union(const QuadArea& other) const noexcept
	{
		QuadArea area(x1,y1,x2,y2);
		if (x1 > other.x1)
			area.x1 = other.x1;
		if (y1 > other.y1)
			area.y1 = other.y1;
		if (area.x2 < other.x2)
			area.x2 = other.x2;
		if (y2 < other.y2)
			area.y2 = other.y2;

		return area;
	}
	constexpr real Left() const noexcept { return x1; }
	constexpr real Top() const noexcept { return y1; }
	constexpr real Right() const noexcept { return x2; }
	constexpr real Bottom() const noexcept { return y2; }
	constexpr real Width() const noexcept { return x2 - x1; }
	constexpr real Height() const noexcept { return y2 - y1; }

	constexpr V2 TopLeft() const noexcept { return V2(x1,y1); }
	constexpr V2 Size() const noexcept { return V2(Width(), Height()); }
	constexpr V2 Center() const noexcept { return V2(x1 + Width()/2, y1 + Height()/2); }

	constexpr void SetWidth(real newWidth) { x2 = x1 + newWidth; }
	constexpr void SetHeight(real newHeight) { y2 = y1 + newHeight; }
	
};

/*=============================================================
 * When constructing a value type and two functions are required.
 *  'getArea(const &T' must return the area of a given value and
 *  'isEqual(const &T, const&T)' must check if the values are equal
 * 
 * A QuadTree has this as its head and a series of linked nodes
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
	const int _MAX_ALLOWED_IN_ONE_NODE = 32; // if more than this many items and _maxDepth not reached and the item's area fits into the children split node to 4 children

	QuadTree(QuadArea area, AreaFor areaFor, Equal isEqual) : _area(area), _AreaFor(areaFor), _Equal(isEqual), _rootNode(std::make_unique<QuadNode>())
	{
			_CalcMaxDepth(_area);
	}

	void Add(const T& value)	// dynamically change the area
	{
		QuadArea areaV = _AreaFor(value);
		if (!_area.Contains(areaV))
		{				  // area always starts at (0,0)
			QuadArea newArea = _area.Union(areaV);
				// to decrease the frequency of resizing enlarge area minimum 1000 points in any direction
			if (newArea.Width() - _area.Width())  // then enlarge x
				newArea.SetWidth(newArea.Width() + 1000);
			if (newArea.Height() - _area.Height())  // then enlarge y
				newArea.SetHeight(newArea.Height() + 1000);
			Resize(newArea, &value); // also adds new value
		}
		else
			_Add(_rootNode.get(), 0, _area, value);
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

	void Resize(QuadArea area, const T* pNewItem = nullptr)
	{
		std::vector<T> values = GetValues(_area);

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

private:
			// functions defined elswhere
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
	int _maxDepth = 8;
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

	void _Add(QuadNode* node, std::size_t depth, const QuadArea& area, const T& value)
	{
		assert(node != nullptr);
		QuadArea areaV = _AreaFor(value);
		assert(area.Contains(areaV));
		if (_IsLeaf(node))
		{
			// V2 childAreaSize = area.Size() / real(2);
			bool insertIntoThisNode =
				(node->values.size() < _MAX_ALLOWED_IN_ONE_NODE) ||	   // when there is space
				(depth == _maxDepth); // ||								   // or no more split possible
				// (childAreaSize.x <= areaV.Width() || childAreaSize.y <= areaV.Height()); // or does not fit into any children
			
			if (insertIntoThisNode)
				node->values.push_back(value);
			else  // Otherwise, we split and we try again
			{
				_Split(node, area);
				_Add(node, depth, area, value);	// still may not be added to any children
			}
		}
		else
		{
			auto i = _GetQuadrant(area, areaV);
			// Add the value to a child if the value is entirely contained in it
			if (i != -1)
			//{ // DEBUG
			//	QuadArea chArea = _AreaOfThisChild(area, i);
			//	QuadNode* pn = node->children[static_cast<std::size_t>(i)].get();
			//	_Add(pn, depth + 1, chArea, value);
			//}
				_Add(node->children[static_cast<std::size_t>(i)].get(), depth + 1, _AreaOfThisChild(area, i), value);
			// Otherwise, we add the value in the current node
			else
				node->values.push_back(value);
		}
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
		assert(it != std::end(node->values) && "Trying to remove a value that is not present in the node");
		// Swap with the last element and pop back
		*it = std::move(node->values.back());
		node->values.pop_back();
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

	int _BottomItem(QuadNode* node, T &item, int bottomY) // returns y for bottom item + item in item
	{
		for (auto v : node->values)
		{
			QuadArea av = _AreaFor(v);
			if (av.Bottom() > bottomY)
			{
				item = v;
				bottomY = av.Bottom();
			}
		}

		if (!_IsLeaf(node))
		{
			bottomY = _BottomItem(node->children[0].get(), item, bottomY);
			bottomY = _BottomItem(node->children[1].get(), item, bottomY);
			bottomY = _BottomItem(node->children[2].get(), item, bottomY);
			bottomY = _BottomItem(node->children[3].get(), item, bottomY);
		}
		return bottomY;
	}
};


#endif
