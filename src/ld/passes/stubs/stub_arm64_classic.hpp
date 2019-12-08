/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Created by Slav Ishutin on 08/10/2018.
 *
 */


// already in ld::passes::stubs namespace
namespace arm64 {
namespace classic {

class StubHelperAtom : public ld::Atom {
public:
											StubHelperAtom(ld::passes::stubs::Pass& pass, const ld::Atom* lazyPointer,
																							const ld::Atom& stubTo, bool forLazyDylib)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStubHelper,
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)),
				_stubTo(stubTo),
				_fixup1(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressARM64Page21, lazyPointer),
				_fixup2(4, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressARM64PageOff12, lazyPointer),
				_fixup3(8, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressARM64Branch26,
					forLazyDylib ? pass.internal()->lazyBindingHelper : pass.internal()->classicBindingHelper)  {
						pass.addAtom(*this);
				}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 12; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		OSWriteLittleInt32(&buffer[0], 0, 0x90000010); // ADRP  X16, lazy_pointer@page
		OSWriteLittleInt32(&buffer[4], 0, 0x91000210); // ADD	X16, X16, lazy_pointer@pageoff
		OSWriteLittleInt32(&buffer[8], 0, 0x14000000); // B     dyld_stub_binding_helper/dyld_lazy_dylib_stub_binding_helper
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd() const				{ return &((ld::Fixup*)&_fixup3)[1]; }

private:

	const ld::Atom&							_stubTo;
	mutable ld::Fixup						_fixup1;
			ld::Fixup						_fixup2;
			ld::Fixup						_fixup3;

	static ld::Section						_s_section;
};

ld::Section						StubHelperAtom::_s_section("__TEXT", "__stub_helper", ld::Section::typeStubHelper);


class LazyPointerAtom : public ld::Atom {
public:
											LazyPointerAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo,
															bool forLazyDylib, bool weakImport)
				: ld::Atom(forLazyDylib ? _s_sectionLazy : _s_section,
							ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeTranslationUnit, ld::Atom::typeLazyPointer,
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(3)),
				_stubTo(stubTo),
				_helper(pass, this, stubTo, forLazyDylib),
				_fixup1(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian64, &_helper),
				_fixup2(0, ld::Fixup::k1of1, ld::Fixup::kindLazyTarget, &stubTo) {
						_fixup2.weakImport = weakImport; pass.addAtom(*this);
					}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 8; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup2)[1]; }

private:

	const ld::Atom&							_stubTo;
	StubHelperAtom							_helper;
	mutable ld::Fixup						_fixup1;
	ld::Fixup								_fixup2;

	static ld::Section						_s_section;
	static ld::Section						_s_sectionLazy;
};

ld::Section LazyPointerAtom::_s_section("__DATA", "__la_symbol_ptr", ld::Section::typeLazyPointer);
ld::Section LazyPointerAtom::_s_sectionLazy("__DATA", "__ld_symbol_ptr", ld::Section::typeLazyDylibPointer);


class StubAtom : public ld::Atom {
public:
											StubAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo,
													bool forLazyDylib, bool weakImport)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStub,
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)),
				_stubTo(stubTo),
				_lazyPointer(pass, stubTo, forLazyDylib, weakImport),
				_fixup1(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressARM64Page21, &_lazyPointer),
				_fixup2(4, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressARM64PageOff12, &_lazyPointer),
				_fixup3(ld::Fixup::kindLinkerOptimizationHint, LOH_ARM64_ADRP_LDR, 0, 4)
					{ pass.addAtom(*this); 	}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 12; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		OSWriteLittleInt32(&buffer[0], 0, 0x90000010); // ADRP  X16, lazy_pointer@page
		OSWriteLittleInt32(&buffer[4], 0, 0xF9400210); // LDR   X16, [X16, lazy_pointer@pageoff]
		OSWriteLittleInt32(&buffer[8], 0, 0xD61F0200); // BR    X16
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup3)[1]; }

private:
	const ld::Atom&							_stubTo;
	LazyPointerAtom							_lazyPointer;
	mutable ld::Fixup						_fixup1;
	mutable ld::Fixup						_fixup2;
	mutable ld::Fixup						_fixup3;

	static ld::Section						_s_section;
};

ld::Section StubAtom::_s_section("__TEXT", "__stubs", ld::Section::typeStub);

} // namespace classic
} // namespace arm64
